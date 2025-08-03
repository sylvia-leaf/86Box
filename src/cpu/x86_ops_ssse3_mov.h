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

static int
opPALIGNR_xmm_a16(uint32_t fetchdat)
{
    SSE_ENTER();
    SSE_REG src, tmp;

    fetch_ea_16(fetchdat);
    SSE_GETSRC();

    int shift = getbyte() << 3;

    if(shift == 0)
    {
        tmp.q[0] = src.q[0];
        tmp.q[1] = src.q[1];
    }
    else if(shift < 64)
    {
        tmp.q[0] = (src.q[0] >> shift) | (src.q[1] << (64 - shift));
        tmp.q[1] = (src.q[1] >> shift) | (cpu_state.XMM[cpu_reg].q[0] << (64 - shift));
    }
    else if(shift < 128)
    {
        shift -= 64;
        tmp.q[0] = (src.q[1] >> shift) | (cpu_state.XMM[cpu_reg].q[0] << (64 - shift));
        tmp.q[1] = (cpu_state.XMM[cpu_reg].q[0] >> shift) | (cpu_state.XMM[cpu_reg].q[1] << (64 - shift));
    }
    else if(shift < 192)
    {
        shift -= 128;
        tmp.q[0] = (cpu_state.XMM[cpu_reg].q[0] >> shift) | (cpu_state.XMM[cpu_reg].q[1] << (64 - shift));
        tmp.q[1] = (cpu_state.XMM[cpu_reg].q[1] >> shift);
    }
    else if(shift < 256)
    {
        shift -= 192;
        tmp.q[0] = (cpu_state.XMM[cpu_reg].q[1] >> shift);
        tmp.q[1] = 0;
    }
    else
    {
        tmp.q[0] = 0;
        tmp.q[1] = 0;
    }

    cpu_state.XMM[cpu_reg].q[0] = tmp.q[0];
    cpu_state.XMM[cpu_reg].q[1] = tmp.q[1];

    return 0;
}

static int
opPALIGNR_xmm_a32(uint32_t fetchdat)
{
    SSE_ENTER();
    SSE_REG src, tmp;

    fetch_ea_32(fetchdat);
    SSE_GETSRC();

    int shift = getbyte() << 3;

    if(shift == 0)
    {
        tmp.q[0] = src.q[0];
        tmp.q[1] = src.q[1];
    }
    else if(shift < 64)
    {
        tmp.q[0] = (src.q[0] >> shift) | (src.q[1] << (64 - shift));
        tmp.q[1] = (src.q[1] >> shift) | (cpu_state.XMM[cpu_reg].q[0] << (64 - shift));
    }
    else if(shift < 128)
    {
        shift -= 64;
        tmp.q[0] = (src.q[1] >> shift) | (cpu_state.XMM[cpu_reg].q[0] << (64 - shift));
        tmp.q[1] = (cpu_state.XMM[cpu_reg].q[0] >> shift) | (cpu_state.XMM[cpu_reg].q[1] << (64 - shift));
    }
    else if(shift < 192)
    {
        shift -= 128;
        tmp.q[0] = (cpu_state.XMM[cpu_reg].q[0] >> shift) | (cpu_state.XMM[cpu_reg].q[1] << (64 - shift));
        tmp.q[1] = (cpu_state.XMM[cpu_reg].q[1] >> shift);
    }
    else if(shift < 256)
    {
        shift -= 192;
        tmp.q[0] = (cpu_state.XMM[cpu_reg].q[1] >> shift);
        tmp.q[1] = 0;
    }
    else
    {
        tmp.q[0] = 0;
        tmp.q[1] = 0;
    }

    cpu_state.XMM[cpu_reg].q[0] = tmp.q[0];
    cpu_state.XMM[cpu_reg].q[1] = tmp.q[1];

    return 0;
}


static int
opPALIGNR_mm_a16(uint32_t fetchdat)
{
    if(cpu_state.sse_xmm) return opPALIGNR_xmm_a16(fetchdat);
    MMX_ENTER();
    MMX_REG src, *dst;

    fetch_ea_16(fetchdat);
    MMX_GETSRC();

    dst = MMX_GETREGP(cpu_reg);

    int shift = getbyte() << 3;

    if(shift == 0) dst->q = src.q;
    else if(shift < 64) dst->q = (src.q >> shift) | (dst->q << (64 - shift));
    else if(shift < 128) dst->q = dst->q >> (shift - 64);
    else dst->q = 0;

    return 0;
}

static int
opPALIGNR_mm_a32(uint32_t fetchdat)
{
    if(cpu_state.sse_xmm) return opPALIGNR_xmm_a32(fetchdat);
    MMX_ENTER();
    MMX_REG src, *dst;

    fetch_ea_32(fetchdat);
    MMX_GETSRC();

    dst = MMX_GETREGP(cpu_reg);

    int shift = getbyte() << 3;

    if(shift == 0) dst->q = src.q;
    else if(shift < 64) dst->q = (src.q >> shift) | (dst->q << (64 - shift));
    else if(shift < 128) dst->q = dst->q >> (shift - 64);
    else dst->q = 0;

    return 0;
}