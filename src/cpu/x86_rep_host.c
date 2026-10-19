/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          The host's side of the REP string instructions (x86_rep_host.h).
 *
 *          The kernels use only baseline instructions (SSE2 on x86-64, plain
 *          C elsewhere): this file is built for the baseline whatever the
 *          rest of the build targets (cpu/CMakeLists.txt), so a build tuned
 *          for one host still has a true baseline path to fall back on and
 *          to measure against.
 *
 *          Stores go out in the guest's order, chunk by chunk: a later
 *          element of a run is never written before an earlier one, as the
 *          P5's in-order stores have it, not just the P6's looser fast
 *          strings.
 *
 * Authors: Michael Pratte, <mpratte@makefox.group>
 *
 *          Copyright 2026 Michael Pratte.
 */
#include <stdint.h>
#include <string.h>
#include "x86_rep_host.h"

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
#    define REP_HOST_X86 1
#    include <immintrin.h>
#else
#    define REP_HOST_X86 0
#endif

uint8_t rep_host_mode = REP_HOST_OFF;

/* ------------------------------------------------------------------ */
/* Plain C, for hosts without SSE2 and for the ends of SSE2 runs      */
/* ------------------------------------------------------------------ */

static inline uint32_t
rep_elem_load(const uint8_t *p, uint32_t esz)
{
    uint32_t v = 0;

    memcpy(&v, p, esz);
    return v;
}

static void
fill_c(uint8_t *p, uint32_t value, uint32_t n, uint32_t esz, int down)
{
    if (down) {
        for (uint8_t *q = p + (n - 1) * esz; n--; q -= esz)
            memcpy(q, &value, esz);
    } else {
        for (; n--; p += esz)
            memcpy(p, &value, esz);
    }
}

static uint32_t
scan_c(const uint8_t *lo, uint32_t n, uint32_t esz, int down, uint32_t value, int repe)
{
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *p = down ? lo + (n - 1 - i) * esz : lo + i * esz;

        if ((rep_elem_load(p, esz) == value) != repe)
            return i;
    }
    return n;
}

static uint32_t
cmp_c(const uint8_t *alo, const uint8_t *blo, uint32_t n, uint32_t esz, int down, int repe)
{
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t o = down ? (n - 1 - i) * esz : i * esz;

        if ((memcmp(alo + o, blo + o, esz) == 0) != repe)
            return i;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Generic x86-64: SSE2, sixteen bytes at a time                      */
/* ------------------------------------------------------------------ */

#if REP_HOST_X86
static void
copy_generic(uint8_t *d, const uint8_t *s, uint32_t n, int down)
{
    if (!down) {
        for (; n >= 16; n -= 16, d += 16, s += 16)
            _mm_storeu_si128((__m128i *) d, _mm_loadu_si128((const __m128i *) s));
    } else {
        for (; n >= 16; n -= 16)
            _mm_storeu_si128((__m128i *) (d + n - 16), _mm_loadu_si128((const __m128i *) (s + n - 16)));
    }
    /* The last few bytes, still in order: memmove copies them as the
       guest would, since the direction is the safe one. */
    if (n)
        memmove(d, s, n);
}

static __m128i
splat_generic(uint32_t value, uint32_t esz)
{
    if (esz == 1)
        return _mm_set1_epi8((char) value);
    if (esz == 2)
        return _mm_set1_epi16((short) value);
    return _mm_set1_epi32((int) value);
}

static void
fill_generic(uint8_t *p, uint32_t value, uint32_t n, uint32_t esz, int down)
{
    const __m128i v     = splat_generic(value, esz);
    const uint32_t per  = 16 / esz;
    uint32_t       left = n;

    if (!down) {
        for (; left >= per; left -= per, p += 16)
            _mm_storeu_si128((__m128i *) p, v);
        fill_c(p, value, left, esz, 0);
    } else {
        for (; left >= per; left -= per)
            _mm_storeu_si128((__m128i *) (p + (left - per) * esz), v);
        fill_c(p, value, left, esz, 1);
    }
}

/* Bit b set where byte b of the chunk belongs to an element that stops
   the run; an element's bytes are all set or all clear. */
static uint32_t
stops_generic(__m128i a, __m128i b, uint32_t esz, int repe)
{
    __m128i  eq;
    uint32_t m;

    if (esz == 1)
        eq = _mm_cmpeq_epi8(a, b);
    else if (esz == 2)
        eq = _mm_cmpeq_epi16(a, b);
    else
        eq = _mm_cmpeq_epi32(a, b);
    m = (uint32_t) _mm_movemask_epi8(eq);
    return repe ? (~m & 0xffff) : m;
}

static uint32_t
scan_generic(const uint8_t *lo, uint32_t n, uint32_t esz, int down, uint32_t value, int repe)
{
    const __m128i  v   = splat_generic(value, esz);
    const uint32_t per = 16 / esz;
    uint32_t       done;

    for (done = 0; (n - done) >= per; done += per) {
        const uint8_t *p = down ? lo + (n - done - per) * esz : lo + done * esz;
        uint32_t       s = stops_generic(_mm_loadu_si128((const __m128i *) p), v, esz, repe);

        if (s)
            return done + (down ? (per - 1 - (31 - __builtin_clz(s)) / esz) : (uint32_t) __builtin_ctz(s) / esz);
    }
    return done + scan_c(down ? lo : lo + done * esz, n - done, esz, down, value, repe);
}

static uint32_t
cmp_generic(const uint8_t *alo, const uint8_t *blo, uint32_t n, uint32_t esz, int down, int repe)
{
    const uint32_t per = 16 / esz;
    uint32_t       done;

    for (done = 0; (n - done) >= per; done += per) {
        const uint32_t o = down ? (n - done - per) * esz : done * esz;
        uint32_t       s = stops_generic(_mm_loadu_si128((const __m128i *) (alo + o)),
                                         _mm_loadu_si128((const __m128i *) (blo + o)), esz, repe);

        if (s)
            return done + (down ? (per - 1 - (31 - __builtin_clz(s)) / esz) : (uint32_t) __builtin_ctz(s) / esz);
    }
    {
        const uint32_t o = down ? 0 : done * esz;

        return done + cmp_c(alo + o, blo + o, n - done, esz, down, repe);
    }
}
#endif

/* ------------------------------------------------------------------ */
/* The entry points                                                   */
/* ------------------------------------------------------------------ */

void
rep_host_copy(uint8_t *dlo, const uint8_t *slo, uint32_t bytes, int down)
{
#if REP_HOST_X86
    copy_generic(dlo, slo, bytes, down);
#else
    (void) down;
    memmove(dlo, slo, bytes);
#endif
}

void
rep_host_copy_elems(uint8_t *d0, const uint8_t *s0, uint32_t n, uint32_t esz, int down)
{
    const intptr_t step = down ? -(intptr_t) esz : (intptr_t) esz;

    /* Each element read whole and then written, as the guest does, so
       what an earlier element wrote is what a later one reads. */
    for (; n--; d0 += step, s0 += step) {
        const uint32_t v = rep_elem_load(s0, esz);

        memcpy(d0, &v, esz);
    }
}

void
rep_host_fill(uint8_t *dlo, uint32_t value, uint32_t n, uint32_t esz, int down)
{
#if REP_HOST_X86
    fill_generic(dlo, value, n, esz, down);
#else
    fill_c(dlo, value, n, esz, down);
#endif
}

uint32_t
rep_host_scan(const uint8_t *lo, uint32_t n, uint32_t esz, int down, uint32_t value, int repe)
{
#if REP_HOST_X86
    return scan_generic(lo, n, esz, down, value, repe);
#else
    return scan_c(lo, n, esz, down, value, repe);
#endif
}

uint32_t
rep_host_cmp(const uint8_t *alo, const uint8_t *blo, uint32_t n, uint32_t esz, int down, int repe)
{
#if REP_HOST_X86
    return cmp_generic(alo, blo, n, esz, down, repe);
#else
    return cmp_c(alo, blo, n, esz, down, repe);
#endif
}

void
rep_host_detect(void)
{
    rep_host_mode = REP_HOST_GENERIC;
}
