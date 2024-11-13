/* SPDX-License-Identifier: GPL-2.0-or-later */
static int
opUCOMISD_xmm_xmm_a16(uint32_t fetchdat)
{
    flags_rebuild();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    cpu_state.flags &= ~(V_FLAG | A_FLAG | N_FLAG | Z_FLAG | P_FLAG | C_FLAG);
    int relation = f64_compare_normal(cpu_state_high.XMM[cpu_reg].d[0], src.d[0], &status);
    FPU_write_eflags_fpu_compare(relation);
    return 0;
}

static int
opUCOMISD_xmm_xmm_a32(uint32_t fetchdat)
{
    // TODO: Unordered result.
    flags_rebuild();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    cpu_state.flags &= ~(V_FLAG | A_FLAG | N_FLAG | Z_FLAG | P_FLAG | C_FLAG);
    int relation = f64_compare_normal(cpu_state_high.XMM[cpu_reg].d[0], src.d[0], &status);
    FPU_write_eflags_fpu_compare(relation);
    return 0;
}

static int
opCOMISD_xmm_xmm_a16(uint32_t fetchdat)
{
    flags_rebuild();
    fetch_ea_16(fetchdat);
    SSE_REG src;
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    cpu_state.flags &= ~(V_FLAG | A_FLAG | N_FLAG | Z_FLAG | P_FLAG | C_FLAG);
    int relation = f64_compare_normal(cpu_state_high.XMM[cpu_reg].d[0], src.d[0], &status);
    FPU_write_eflags_fpu_compare(relation);
    return 0;
}

static int
opCOMISD_xmm_xmm_a32(uint32_t fetchdat)
{
    flags_rebuild();
    fetch_ea_32(fetchdat);
    SSE_REG src;
    struct softfloat_status_t status = mxcsr_to_softfloat_status_word();
    SSE_GETSRC();
    cpu_state.flags &= ~(V_FLAG | A_FLAG | N_FLAG | Z_FLAG | P_FLAG | C_FLAG);
    int relation = f64_compare_normal(cpu_state_high.XMM[cpu_reg].d[0], src.d[0], &status);
    FPU_write_eflags_fpu_compare(relation);
    return 0;
}
