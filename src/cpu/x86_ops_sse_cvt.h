/* SPDX-License-Identifier: GPL-2.0-or-later */
static int
opCVTPI2PS_xmm_mm_a16(uint32_t fetchdat)
{
    feclearexcept(FE_ALL_EXCEPT);
    MMX_REG src;

    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opCVTPI2PD_xmm_mm_a16(fetchdat);

    MMX_ENTER();
    fetch_ea_16(fetchdat);
    MMX_GETSRC();
    fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
    cpu_state_high.XMM[cpu_reg].f[0] = src.sl[0];
    cpu_state_high.XMM[cpu_reg].f[1] = src.sl[1];
    fesetround(FE_TONEAREST);
    check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
    check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[1]);
    CLOCK_CYCLES(1);

    return 0;
}

static int
opCVTPI2PS_xmm_mm_a32(uint32_t fetchdat)
{
    feclearexcept(FE_ALL_EXCEPT);
    MMX_REG src;

    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opCVTPI2PD_xmm_mm_a32(fetchdat);

    MMX_ENTER();
    fetch_ea_32(fetchdat);
    MMX_GETSRC();
    fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
    cpu_state_high.XMM[cpu_reg].f[0] = src.sl[0];
    cpu_state_high.XMM[cpu_reg].f[1] = src.sl[1];
    fesetround(FE_TONEAREST);
    check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
    check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[1]);
    CLOCK_CYCLES(1);

    return 0;
}

static int
opCVTSI2SS_xmm_l_a16(uint32_t fetchdat)
{
    feclearexcept(FE_ALL_EXCEPT);
    fetch_ea_16(fetchdat);
    if (cpu_mod == 3) {
        fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
        cpu_state_high.XMM[cpu_reg].f[0] = (int32_t)getr32(cpu_rm);
        fesetround(FE_TONEAREST);
        check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
        CLOCK_CYCLES(1);
    } else {
        uint32_t dst;

        SEG_CHECK_READ(cpu_state.ea_seg);
        dst = readmeml(easeg, cpu_state.eaaddr);
        if (cpu_state.abrt)
            return 1;
        fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
        cpu_state_high.XMM[cpu_reg].f[0] = (int32_t)dst;
        fesetround(FE_TONEAREST);
        check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
        CLOCK_CYCLES(2);
    }

    return 0;
}

static int
opCVTSI2SS_xmm_l_a32(uint32_t fetchdat)
{
    feclearexcept(FE_ALL_EXCEPT);
    fetch_ea_32(fetchdat);
    if (cpu_mod == 3) {
        fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
        cpu_state_high.XMM[cpu_reg].f[0] = (int32_t)getr32(cpu_rm);
        fesetround(FE_TONEAREST);
        check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
        CLOCK_CYCLES(1);
    } else {
        uint32_t dst;

        SEG_CHECK_READ(cpu_state.ea_seg);
        dst = readmeml(easeg, cpu_state.eaaddr);
        if (cpu_state.abrt)
            return 1;
        fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
        cpu_state_high.XMM[cpu_reg].f[0] = (int32_t)dst;
        fesetround(FE_TONEAREST);
        check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
        CLOCK_CYCLES(2);
    }

    return 0;
}

static int
opCVTTPS2PI_mm_xmm_a16(uint32_t fetchdat)
{
    SSE_REG  src;
    MMX_REG *dst;

    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opCVTTPD2PI_mm_xmm_a16(fetchdat);

    MMX_ENTER();
    fetch_ea_16(fetchdat);

    dst = MMX_GETREGP(cpu_reg);
    SSE_GETSRC();
    dst->sl[0] = trunc(src.f[0]);
    dst->sl[1] = trunc(src.f[1]);
    MMX_SETEXP(cpu_reg);
    CLOCK_CYCLES(1);

    return 0;
}

static int
opCVTTPS2PI_mm_xmm_a32(uint32_t fetchdat)
{
    SSE_REG  src;
    MMX_REG *dst;

    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opCVTTPD2PI_mm_xmm_a32(fetchdat);

    MMX_ENTER();
    fetch_ea_32(fetchdat);
    dst = MMX_GETREGP(cpu_reg);
    SSE_GETSRC();
    dst->sl[0] = trunc(src.f[0]);
    dst->sl[1] = trunc(src.f[1]);
    MMX_SETEXP(cpu_reg);
    CLOCK_CYCLES(1);

    return 0;
}

static int
opCVTTSS2SI_l_xmm_a16(uint32_t fetchdat)
{
    feclearexcept(FE_ALL_EXCEPT);
    fetch_ea_16(fetchdat);
    if (cpu_mod == 3) {
        int32_t result = trunc(cpu_state_high.XMM[cpu_rm].f[0]); 
        setr32(cpu_reg, result);
        check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
        CLOCK_CYCLES(1);
    } else {
        uint32_t dst;

        SEG_CHECK_READ(cpu_state.ea_seg);
        dst = readmeml(easeg, cpu_state.eaaddr);
        if (cpu_state.abrt)
            return 1;
        float dst_real;
        dst_real = *(float *) &dst;
        int32_t result = trunc(dst_real);
        setr32(cpu_reg, result);
        check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
        CLOCK_CYCLES(2);
    }

    return 0;
}

static int
opCVTTSS2SI_l_xmm_a32(uint32_t fetchdat)
{
    feclearexcept(FE_ALL_EXCEPT);
    fetch_ea_32(fetchdat);
    if (cpu_mod == 3) {
        int32_t result = trunc(cpu_state_high.XMM[cpu_rm].f[0]); 
        setr32(cpu_reg, result);
        check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
        CLOCK_CYCLES(1);
    } else {
        uint32_t dst;

        SEG_CHECK_READ(cpu_state.ea_seg);
        dst = readmeml(easeg, cpu_state.eaaddr);
        if (cpu_state.abrt)
            return 1;
        float dst_real;
        dst_real = *(float *) &dst;
        int32_t result = trunc(dst_real);
        setr32(cpu_reg, result);
        check_sse_exceptions_float(&cpu_state_high.XMM[cpu_reg].f[0]);
        CLOCK_CYCLES(2);
    }

    return 0;
}

static int
opCVTPS2PI_mm_xmm_a16(uint32_t fetchdat)
{
    SSE_REG  src;
    MMX_REG *dst;

    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opCVTPD2PI_mm_xmm_a16(fetchdat);

    MMX_ENTER();
    fetch_ea_16(fetchdat);
    dst = MMX_GETREGP(cpu_reg);
    SSE_GETSRC();
    fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
    dst->sl[0] = src.f[0];
    dst->sl[1] = src.f[1];
    fesetround(FE_TONEAREST);
    MMX_SETEXP(cpu_reg);
    CLOCK_CYCLES(1);

    return 0;
}

static int
opCVTPS2PI_mm_xmm_a32(uint32_t fetchdat)
{
    SSE_REG  src;
    MMX_REG *dst;

    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opCVTPD2PI_mm_xmm_a32(fetchdat);

    MMX_ENTER();
    fetch_ea_32(fetchdat);
    dst = MMX_GETREGP(cpu_reg);
    SSE_GETSRC();
    fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
    dst->sl[0] = src.f[0];
    dst->sl[1] = src.f[1];
    fesetround(FE_TONEAREST);
    MMX_SETEXP(cpu_reg);
    CLOCK_CYCLES(1);

    return 0;
}

static int
opCVTSS2SI_l_xmm_a16(uint32_t fetchdat)
{
    SSE_REG  src;
    int32_t result;
    fetch_ea_16(fetchdat);
    SSE_GETSRC();
    fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
    result = src.f[0];
    setr32(cpu_reg, result);
    fesetround(FE_TONEAREST);

    return 0;
}

static int
opCVTSS2SI_l_xmm_a32(uint32_t fetchdat)
{
    SSE_REG  src;
    int32_t result;
    fetch_ea_32(fetchdat);
    SSE_GETSRC();
    fesetround(rounding_modes[(cpu_state_high.mxcsr >> 13) & 3]);
    result = src.f[0];
    setr32(cpu_reg, result);
    fesetround(FE_TONEAREST);

    return 0;
}
