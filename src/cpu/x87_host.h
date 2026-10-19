/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          The host-x87 FPU: the softfloat FPU's own handlers, state and
 *          exception handling, with the arithmetic done by the host's x87.
 *
 *          The softfloat FPU keeps each register in the x87's own 80-bit
 *          format (extFloat80_t has the x87's memory layout: the 64-bit
 *          significand, then the sign and exponent), so an operation is: load
 *          the operands onto the host x87 with FLD m80, run the same
 *          instruction, read the status word, store the result with FSTP m80.
 *          Loads, stores, add, subtract, multiply, divide, square root, round
 *          to integer and the conversions to single, double and integers come
 *          out of the host's x87 exactly as a real one gives them, precision
 *          and rounding control included. The transcendentals, FPREM, FSCALE,
 *          FXTRACT and the compares stay with softfloat, whose models follow
 *          the guest processor rather than the host's.
 *
 *          The kernels are machine code the recompiler's backend writes at
 *          startup (codegen_backend_x86-64.c), so no compiler or assembler is
 *          involved: every x86-64 build with the new recompiler has them. The
 *          host x87 runs with every exception masked, so it never traps; the
 *          handlers' FPU_exception() applies the guest's masks as it does for
 *          softfloat. Each kernel saves the host's control word, loads the
 *          guest's only if it differs, and puts the host's back: both host
 *          ABIs preserve the x87 control word across calls, and the C library
 *          on this thread does its exp, pow, log, sin and sqrt on the x87.
 *
 *          FNCLEX, which with the control word load was measured on the
 *          host at nine tenths of each operation, is kept off the
 *          per-operation path. The exception flags are sticky in the guest
 *          as in the host, so while the guest masks all six the host's
 *          accumulated flags are exactly what the guest's status word should
 *          gain; they are cleared only when the guest's own were cleared, or
 *          when the guest unmasks an exception and needs each operation's
 *          flags alone. A kernel reads the status word before its operation
 *          too: a flag set there that the guest was never given came from
 *          something else on the thread, and the operation is run again
 *          after clearing, so no stray flag reaches the guest.
 *
 *          Included by x87_ops.h after softfloat's own headers: it renames the
 *          softfloat calls the handlers make, so that fpu_host_x87 picks the
 *          host's x87 at run time. A build without the kernels (another host,
 *          or no recompiler) renames nothing.
 *
 * Authors: Michael Pratte, <mpratte@makefox.group>
 *
 *          Copyright 2026 Michael Pratte.
 */
#ifndef EMU_X87_HOST_H
#define EMU_X87_HOST_H

#include <stdbool.h>
#include <stdint.h>
#include "x87_sf.h" /* softfloat3e/softfloat.h, fpu_state */

#define X87_HOST_AVAILABLE X87_HOST_BUILD /* <86box/86box.h> */

#if X87_HOST_AVAILABLE
/* What a kernel works on: the operands, the result, the control word to run
   with and the status word it leaves. One pointer is its only argument. The
   kernel puts the host's own control word (hcw) back before it returns: both
   host ABIs preserve the x87 control word across calls, and the C library
   on this thread does its exp, pow, log, sin and sqrt on the x87. */
typedef struct x87_host_frame_t {
    extFloat80_t a;    /* 0x00 */
    extFloat80_t b;    /* 0x10 */
    extFloat80_t r;    /* 0x20 */
    uint64_t     i;    /* 0x30: an integer, single or double result */
    uint16_t     cw;   /* 0x38 */
    uint16_t     sw;   /* 0x3a: after the operation */
    uint16_t     pre;  /* 0x3c: before it */
    uint16_t     hcw;  /* 0x3e: the host's own, as the kernel found it */
} x87_host_frame_t;

#    define X87_HOST_A    0x00
#    define X87_HOST_B    0x10
#    define X87_HOST_R    0x20
#    define X87_HOST_I    0x30
#    define X87_HOST_CW   0x38
#    define X87_HOST_SW   0x3a
#    define X87_HOST_PRE  0x3c
#    define X87_HOST_HCW  0x3e

enum {
    X87_HOST_ADD = 0,
    X87_HOST_SUB,
    X87_HOST_MUL,
    X87_HOST_DIV,
    X87_HOST_SQRT,
    X87_HOST_RNDINT,
    X87_HOST_TO_F32,
    X87_HOST_TO_F64,
    X87_HOST_TO_I32,
    X87_HOST_TO_I64,
    X87_HOST_CLEAR, /* FNCLEX */
    X87_HOST_COUNT
};

typedef void (*x87_host_kernel_t)(x87_host_frame_t *f);

/* Set by the backend when it has written the kernels (none on a build or a
   host without them). */
extern x87_host_kernel_t x87_host_kernel[X87_HOST_COUNT];

/* The host x87 of this thread, in cpu_state: the exception flags it has
   accumulated since they were last cleared, all of which the guest has been
   given (0x3f: not known). A value that is out of date costs nothing but a
   clear: the host's flags are read before every operation, and one the guest
   lacks is cleared. The control word is not kept: each operation loads the
   guest's, when the host's own differs, and puts the host's back. */
#    define x87_host_flags_now cpu_state.x87h_fl

/* The host control word for an operation: every exception masked, the
   precision the guest asked for, and the rounding the operation uses (the
   guest's for arithmetic; its own for a conversion that names one). */
static __inline uint16_t
x87_host_cw(const struct softfloat_status_t *status, int rounding)
{
    const uint16_t pc = (status->extF80_roundingPrecision == 32) ? 0x0000 :
                        (status->extF80_roundingPrecision == 64) ? 0x0200 : 0x0300;

    return (uint16_t) (0x007f | pc | (((rounding > 3) ? 0 : rounding) << 10));
}

/* The status word's exception bits are softfloat's flags; C1, which says a
   result was rounded up, is softfloat's RAISE_SW_C1. While every exception is
   masked the bits are the host's accumulated ones, which the guest's sticky
   status word already holds but for this operation's. */
static __inline void
x87_host_flags(struct softfloat_status_t *status, uint16_t sw, bool exact)
{
    int flags = sw & 0x003f;

    if (sw & 0x0200)
        flags |= RAISE_SW_C1;
    if (!exact)
        flags &= ~(softfloat_flag_inexact | RAISE_SW_C1);
    status->softfloat_exceptionFlags |= flags;
}

static __inline bool
x87_host_on(void)
{
    return fpu_host_x87 && (x87_host_kernel[0] != NULL);
}

/* Runs kernel k on the frame (which loads the control word and puts the
   host's back itself): the flags cleared if the guest needs this operation's
   alone (an exception unmasked) or has cleared some the host still holds,
   and the operation run again after clearing if the host held a flag the
   guest was never given. */
static __inline void
x87_host_run_kernel(x87_host_kernel_t k, x87_host_frame_t *f)
{
    if (((fpu_state.cwd & 0x003f) != 0x003f) || (x87_host_flags_now & ~fpu_state.swd & 0x003f)) {
        x87_host_kernel[X87_HOST_CLEAR](f);
        x87_host_flags_now = 0;
    }
    k(f);
    if (f->pre & ~x87_host_flags_now & 0x003f) {
        x87_host_kernel[X87_HOST_CLEAR](f);
        k(f);
    }
    x87_host_flags_now = f->sw & 0x003f;
}

static __inline void
x87_host_run(int k, x87_host_frame_t *f)
{
    x87_host_run_kernel(x87_host_kernel[k], f);
}

static __inline extFloat80_t
x87_host_binary(int k, extFloat80_t a, extFloat80_t b, struct softfloat_status_t *status)
{
    x87_host_frame_t f;

    f.a  = a;
    f.b  = b;
    f.cw = x87_host_cw(status, status->softfloat_roundingMode);
    x87_host_run(k, &f);
    x87_host_flags(status, f.sw, true);
    return f.r;
}

static __inline extFloat80_t
x87_host_unary(int k, extFloat80_t a, int rounding, bool exact, struct softfloat_status_t *status)
{
    x87_host_frame_t f;

    f.a  = a;
    f.cw = x87_host_cw(status, rounding);
    x87_host_run(k, &f);
    x87_host_flags(status, f.sw, exact);
    return f.r;
}

static __inline uint64_t
x87_host_convert(int k, extFloat80_t a, int rounding, bool exact, struct softfloat_status_t *status)
{
    x87_host_frame_t f;

    f.a  = a;
    f.cw = x87_host_cw(status, rounding);
    x87_host_run(k, &f);
    x87_host_flags(status, f.sw, exact);
    return f.i;
}

/* ------------------------------------------------------------------ */
/* The calls the handlers make                                        */
/* ------------------------------------------------------------------ */

static __inline extFloat80_t
x87h_add(extFloat80_t a, extFloat80_t b, struct softfloat_status_t *status)
{
    return x87_host_on() ? x87_host_binary(X87_HOST_ADD, a, b, status) : extF80_add(a, b, status);
}
static __inline extFloat80_t
x87h_sub(extFloat80_t a, extFloat80_t b, struct softfloat_status_t *status)
{
    return x87_host_on() ? x87_host_binary(X87_HOST_SUB, a, b, status) : extF80_sub(a, b, status);
}
static __inline extFloat80_t
x87h_mul(extFloat80_t a, extFloat80_t b, struct softfloat_status_t *status)
{
    return x87_host_on() ? x87_host_binary(X87_HOST_MUL, a, b, status) : extF80_mul(a, b, status);
}
static __inline extFloat80_t
x87h_div(extFloat80_t a, extFloat80_t b, struct softfloat_status_t *status)
{
    return x87_host_on() ? x87_host_binary(X87_HOST_DIV, a, b, status) : extF80_div(a, b, status);
}
static __inline extFloat80_t
x87h_sqrt(extFloat80_t a, struct softfloat_status_t *status)
{
    return x87_host_on() ? x87_host_unary(X87_HOST_SQRT, a, status->softfloat_roundingMode, true, status) :
                           extF80_sqrt(a, status);
}
static __inline extFloat80_t
x87h_round_to_int(extFloat80_t a, uint8_t rounding, bool exact, struct softfloat_status_t *status)
{
    return x87_host_on() ? x87_host_unary(X87_HOST_RNDINT, a, rounding, exact, status) :
                           extF80_roundToInt(a, rounding, exact, status);
}
static __inline float32
x87h_to_f32(extFloat80_t a, struct softfloat_status_t *status)
{
    return x87_host_on() ? (float32) x87_host_convert(X87_HOST_TO_F32, a, status->softfloat_roundingMode, true, status) :
                           extF80_to_f32(a, status);
}
static __inline float64
x87h_to_f64(extFloat80_t a, struct softfloat_status_t *status)
{
    return x87_host_on() ? (float64) x87_host_convert(X87_HOST_TO_F64, a, status->softfloat_roundingMode, true, status) :
                           extF80_to_f64(a, status);
}
static __inline int32_t
x87h_to_i32(extFloat80_t a, uint8_t rounding, bool exact, struct softfloat_status_t *status)
{
    return x87_host_on() ? (int32_t) (uint32_t) x87_host_convert(X87_HOST_TO_I32, a, rounding, exact, status) :
                           extF80_to_i32(a, rounding, exact, status);
}
static __inline int64_t
x87h_to_i64(extFloat80_t a, uint8_t rounding, bool exact, struct softfloat_status_t *status)
{
    return x87_host_on() ? (int64_t) x87_host_convert(X87_HOST_TO_I64, a, rounding, exact, status) :
                           extF80_to_i64(a, rounding, exact, status);
}
static __inline int32_t
x87h_to_i32_r_minMag(extFloat80_t a, bool exact, struct softfloat_status_t *status)
{
    return x87_host_on() ? (int32_t) (uint32_t) x87_host_convert(X87_HOST_TO_I32, a, softfloat_round_minMag, exact, status) :
                           extF80_to_i32_r_minMag(a, exact, status);
}
static __inline int64_t
x87h_to_i64_r_minMag(extFloat80_t a, bool exact, struct softfloat_status_t *status)
{
    return x87_host_on() ? (int64_t) x87_host_convert(X87_HOST_TO_I64, a, softfloat_round_minMag, exact, status) :
                           extF80_to_i64_r_minMag(a, exact, status);
}

/* A 16-bit integer store: as softfloat does it, through 32 bits, with an
   out-of-range value invalid and the integer indefinite stored. */
static __inline int16_t
x87h_to_i16_from(int32_t v32, struct softfloat_status_t *status)
{
    const int16_t v16 = (int16_t) v32;

    if ((int32_t) v16 != v32) {
        softfloat_setFlags(status, softfloat_flag_invalid);
        return (int16_t) 0x8000;
    }
    return v16;
}

/* The handlers' calls, renamed. softfloat.h's own inline wrappers call the
   library directly, so they are renamed here too. */
#    define extF80_add(a, b, s)                      x87h_add(a, b, s)
#    define extF80_sub(a, b, s)                      x87h_sub(a, b, s)
#    define extF80_mul(a, b, s)                      x87h_mul(a, b, s)
#    define extF80_div(a, b, s)                      x87h_div(a, b, s)
#    define extF80_sqrt(a, s)                        x87h_sqrt(a, s)
#    define extF80_roundToInt(a, r, e, s)            x87h_round_to_int(a, r, e, s)
#    define extF80_roundToInt_normal(a, s)           x87h_round_to_int(a, softfloat_getRoundingMode(s), true, s)
#    define extF80_to_f32(a, s)                      x87h_to_f32(a, s)
#    define extF80_to_f64(a, s)                      x87h_to_f64(a, s)
#    define extF80_to_i32(a, r, e, s)                x87h_to_i32(a, r, e, s)
#    define extF80_to_i64(a, r, e, s)                x87h_to_i64(a, r, e, s)
#    define extF80_to_i32_normal(a, s)               x87h_to_i32(a, softfloat_getRoundingMode(s), true, s)
#    define extF80_to_i64_normal(a, s)               x87h_to_i64(a, softfloat_getRoundingMode(s), true, s)
#    define extF80_to_i16(a, s)                      x87h_to_i16_from(x87h_to_i32(a, softfloat_getRoundingMode(s), true, s), s)
#    define extF80_to_i32_round_to_zero(a, s)        x87h_to_i32_r_minMag(a, true, s)
#    define extF80_to_i64_round_to_zero(a, s)        x87h_to_i64_r_minMag(a, true, s)
#    define extF80_to_i16_round_to_zero(a, s)        x87h_to_i16_from(x87h_to_i32_r_minMag(a, true, s), s)

#endif /* X87_HOST_AVAILABLE */

#endif /*EMU_X87_HOST_H*/
