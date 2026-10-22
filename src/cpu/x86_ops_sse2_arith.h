/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdlib.h>
#include <math.h>

static int
opSQRTPD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_sqrt(src.d[0], &status);
    result.d[1] = f64_sqrt(src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opSQRTPD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_sqrt(src.d[0], &status);
    result.d[1] = f64_sqrt(src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opSQRTSD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_sqrt(src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opSQRTSD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_sqrt(src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opADDPD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_add(result.d[0], src.d[0], &status);
    result.d[1] = f64_add(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opADDPD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_add(result.d[0], src.d[0], &status);
    result.d[1] = f64_add(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opADDSD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_add(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opADDSD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_add(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMULPD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_mul(result.d[0], src.d[0], &status);
    result.d[1] = f64_mul(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMULPD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_mul(result.d[0], src.d[0], &status);
    result.d[1] = f64_mul(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMULSD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_mul(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMULSD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_mul(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opSUBPD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_sub(result.d[0], src.d[0], &status);
    result.d[1] = f64_sub(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opSUBPD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_sub(result.d[0], src.d[0], &status);
    result.d[1] = f64_sub(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opSUBSD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_sub(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opSUBSD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_sub(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMINPD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_min(result.d[0], src.d[0], &status);
    result.d[1] = f64_min(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMINPD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_min(result.d[0], src.d[0], &status);
    result.d[1] = f64_min(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMINSD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_min(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMINSD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_min(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opDIVPD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_div(result.d[0], src.d[0], &status);
    result.d[1] = f64_div(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opDIVPD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_div(result.d[0], src.d[0], &status);
    result.d[1] = f64_div(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opDIVSD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_div(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opDIVSD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_div(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMAXPD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_max(result.d[0], src.d[0], &status);
    result.d[1] = f64_max(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMAXPD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.d[0] = f64_max(result.d[0], src.d[0], &status);
    result.d[1] = f64_max(result.d[1], src.d[1], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMAXSD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_max(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opMAXSD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.d[0] = f64_max(result.d[0], src.d[0], &status);
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

/* Comparison predicate for CMPSD/CMPPD instructions */
static float64_compare_method compare64[8] = {
  f64_eq_ordered_quiet,
  f64_lt_ordered_signalling,
  f64_le_ordered_signalling,
  f64_unordered_quiet,
  f64_neq_unordered_quiet,
  f64_nlt_unordered_signalling,
  f64_nle_unordered_signalling,
  f64_ordered_quiet
};

static int
opCMPPD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    uint8_t  imm = getbyte();
    result.q[0] = compare64[imm & 7](result.d[0], src.d[0], &status) ? ~0ull : 0;
    result.q[1] = compare64[imm & 7](result.d[1], src.d[1], &status) ? ~0ull : 0;
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opCMPPD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    uint8_t  imm = getbyte();
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    result.q[0] = compare64[imm & 7](result.d[0], src.d[0], &status) ? ~0ull : 0;
    result.q[1] = compare64[imm & 7](result.d[1], src.d[1], &status) ? ~0ull : 0;
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opCMPSD_xmm_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_16(fetchdat);
    uint8_t  imm = getbyte();
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.q[0] = compare64[imm & 7](result.d[0], src.d[0], &status) ? ~0ull : 0;
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}

static int
opCMPSD_xmm_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    fetch_ea_32(fetchdat);
    uint8_t  imm = getbyte();
    SSE_REG src;
    SSE_REG result = cpu_state.XMM[cpu_reg];
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC64();
    result.q[0] = compare64[imm & 7](result.d[0], src.d[0], &status) ? ~0ull : 0;
    if (softfloat_status_word_to_mxcsr(status))
        return 1;
    cpu_state.XMM[cpu_reg] = result;
    return 0;
}
