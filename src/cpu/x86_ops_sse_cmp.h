/* SPDX-License-Identifier: GPL-2.0-or-later */
static int
opUCOMISS_xmm_xmm_a16(uint32_t fetchdat)
{
    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opUCOMISD_xmm_xmm_a16(fetchdat);

    flags_rebuild();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    cpu_state.flags &= ~(V_FLAG | A_FLAG | N_FLAG | Z_FLAG | P_FLAG | C_FLAG);
    int relation = f32_compare(cpu_state_high.XMM[cpu_reg].f[0], src.f[0], &status);
    FPU_write_eflags_fpu_compare(relation);
    return 0;
}

static int
opUCOMISS_xmm_xmm_a32(uint32_t fetchdat)
{
    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opUCOMISD_xmm_xmm_a32(fetchdat);

    flags_rebuild();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    cpu_state.flags &= ~(V_FLAG | A_FLAG | N_FLAG | Z_FLAG | P_FLAG | C_FLAG);
    int relation = f32_compare(cpu_state_high.XMM[cpu_reg].f[0], src.f[0], &status);
    FPU_write_eflags_fpu_compare(relation);
    return 0;
}

static int
opCOMISS_xmm_xmm_a16(uint32_t fetchdat)
{
    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opCOMISD_xmm_xmm_a16(fetchdat);

    flags_rebuild();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    cpu_state.flags &= ~(V_FLAG | A_FLAG | N_FLAG | Z_FLAG | P_FLAG | C_FLAG);
    int relation = f32_compare(cpu_state_high.XMM[cpu_reg].f[0], src.f[0], &status);
    FPU_write_eflags_fpu_compare(relation);
    return 0;
}

static int
opCOMISS_xmm_xmm_a32(uint32_t fetchdat)
{
    if ((cpu_features & CPU_FEATURE_SSE2) && sse_xmm)
        return opCOMISD_xmm_xmm_a32(fetchdat);

    flags_rebuild();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    cpu_state.flags &= ~(V_FLAG | A_FLAG | N_FLAG | Z_FLAG | P_FLAG | C_FLAG);
    int relation = f32_compare(cpu_state_high.XMM[cpu_reg].f[0], src.f[0], &status);
    FPU_write_eflags_fpu_compare(relation);
    return 0;
}
