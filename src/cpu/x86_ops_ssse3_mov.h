/* SPDX-License-Identifier: GPL-2.0-or-later */
static int
opPSHUFB_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    SSE_REG src, tmp;

    fetch_ea_16(fetchdat);
    SSE_GETSRC();

    tmp = cpu_state.XMM[cpu_reg];

    for(int i = 0; i < 16; i++)
    {
        if(src.b[i] & 0x80)
        {
            cpu_state.XMM[cpu_reg].b[i] = 0;
        }
        else
        {
            cpu_state.XMM[cpu_reg].b[i] = tmp.b[src.b[i] & 0xf];
        }
    }
    return 0;
}

static int
opPSHUFB_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    SSE_REG src, tmp;

    fetch_ea_32(fetchdat);
    SSE_GETSRC();

    tmp = cpu_state.XMM[cpu_reg];

    for(int i = 0; i < 16; i++)
    {
        if(src.b[i] & 0x80)
        {
            cpu_state.XMM[cpu_reg].b[i] = 0;
        }
        else
        {
            cpu_state.XMM[cpu_reg].b[i] = tmp.b[src.b[i] & 0xf];
        }
    }
    return 0;
}

static int
opPSHUFB_mm_a16(uint32_t fetchdat)
{
    if(cpu_state.sse_xmm) return opPSHUFB_xmm_a16(fetchdat);
    MMX_ENTER();
    MMX_REG src, *dst, tmp;

    fetch_ea_16(fetchdat);
    MMX_GETSRC();

    dst = MMX_GETREGP(cpu_reg);

    tmp = *dst;

    for(int i = 0; i < 8; i++)
    {
        if(src.b[i] & 0x80)
        {
            dst->b[i] = 0;
        }
        else
        {
            dst->b[i] = tmp.b[src.b[i] & 7];
        }
    }
    return 0;
}

static int
opPSHUFB_mm_a32(uint32_t fetchdat)
{
    if(cpu_state.sse_xmm) return opPSHUFB_xmm_a32(fetchdat);
    MMX_ENTER();
    MMX_REG src, *dst, tmp;

    fetch_ea_32(fetchdat);
    MMX_GETSRC();

    dst = MMX_GETREGP(cpu_reg);

    tmp = *dst;

    for(int i = 0; i < 8; i++)
    {
        if(src.b[i] & 0x80)
        {
            dst->b[i] = 0;
        }
        else
        {
            dst->b[i] = tmp.b[src.b[i] & 7];
        }
    }
    return 0;
}