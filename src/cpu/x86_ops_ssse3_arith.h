/* SPDX-License-Identifier: GPL-2.0-or-later */
static int
opPHADDW_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    SSE_REG src, tmp;

    fetch_ea_16(fetchdat);
    SSE_GETSRC();

    tmp.w[0] = cpu_state.XMM[cpu_reg].w[1] + cpu_state.XMM[cpu_reg].w[0];
    tmp.w[1] = cpu_state.XMM[cpu_reg].w[3] + cpu_state.XMM[cpu_reg].w[2];
    tmp.w[2] = cpu_state.XMM[cpu_reg].w[5] + cpu_state.XMM[cpu_reg].w[4];
    tmp.w[3] = cpu_state.XMM[cpu_reg].w[7] + cpu_state.XMM[cpu_reg].w[6];
    tmp.w[4] = src.w[1] + src.w[0];
    tmp.w[5] = src.w[3] + src.w[2];
    tmp.w[6] = src.w[5] + src.w[4];
    tmp.w[7] = src.w[7] + src.w[6];

    cpu_state.XMM[cpu_reg].q[0] = tmp.q[0];
    cpu_state.XMM[cpu_reg].q[1] = tmp.q[1];
    return 0;
}

static int
opPHADDW_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    SSE_REG src, tmp;

    fetch_ea_32(fetchdat);
    SSE_GETSRC();

    tmp.w[0] = cpu_state.XMM[cpu_reg].w[1] + cpu_state.XMM[cpu_reg].w[0];
    tmp.w[1] = cpu_state.XMM[cpu_reg].w[3] + cpu_state.XMM[cpu_reg].w[2];
    tmp.w[2] = cpu_state.XMM[cpu_reg].w[5] + cpu_state.XMM[cpu_reg].w[4];
    tmp.w[3] = cpu_state.XMM[cpu_reg].w[7] + cpu_state.XMM[cpu_reg].w[6];
    tmp.w[4] = src.w[1] + src.w[0];
    tmp.w[5] = src.w[3] + src.w[2];
    tmp.w[6] = src.w[5] + src.w[4];
    tmp.w[7] = src.w[7] + src.w[6];

    cpu_state.XMM[cpu_reg].q[0] = tmp.q[0];
    cpu_state.XMM[cpu_reg].q[1] = tmp.q[1];
    return 0;
}

static int
opPHADDW_mm_a16(uint32_t fetchdat)
{
    if(sse_xmm) return opPHADDW_xmm_a16(fetchdat);
    MMX_ENTER();
    MMX_REG src, *dst, tmp;

    fetch_ea_16(fetchdat);
    MMX_GETSRC();

    dst = MMX_GETREGP(cpu_reg);

    tmp.w[0] = dst->w[1] + dst->w[0];
    tmp.w[1] = dst->w[3] + dst->w[2];
    tmp.w[2] = src.w[1] + src.w[0];
    tmp.w[3] = src.w[3] + src.w[2];

    dst->q = tmp.q;
    return 0;
}

static int
opPHADDW_mm_a32(uint32_t fetchdat)
{
    if(sse_xmm) return opPHADDW_xmm_a32(fetchdat);
    MMX_ENTER();
    MMX_REG src, *dst, tmp;

    fetch_ea_32(fetchdat);
    MMX_GETSRC();

    dst = MMX_GETREGP(cpu_reg);

    tmp.w[0] = dst->w[1] + dst->w[0];
    tmp.w[1] = dst->w[3] + dst->w[2];
    tmp.w[2] = src.w[1] + src.w[0];
    tmp.w[3] = src.w[3] + src.w[2];

    dst->q = tmp.q;
    return 0;
}