static int opPADDQ_a16(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();

        fetch_ea_16(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].q += src.q;

        return 0;
}

static int opPADDQ_a32(uint32_t fetchdat)
{
        MMX_REG src;
        MMX_ENTER();

        fetch_ea_32(fetchdat);
        MMX_GETSRC();

        cpu_state.MM[cpu_reg].q += src.q;

        return 0;
}

static int opPADDQ_xmm_a16(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_16(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].q[0] += src.q[0];
        XMM[cpu_reg].q[1] += src.q[1];

        return 0;
}

static int opPADDQ_xmm_a32(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_32(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].q[0] += src.q[0];
        XMM[cpu_reg].q[1] += src.q[1];

        return 0;
}

static int opPMULLW_xmm_a16(uint32_t fetchdat)
{
        fetch_ea_16(fetchdat);
        if (cpu_mod == 3)
        {
                XMM[cpu_reg].w[0] *= XMM[cpu_rm].w[0];
                XMM[cpu_reg].w[1] *= XMM[cpu_rm].w[1];
                XMM[cpu_reg].w[2] *= XMM[cpu_rm].w[2];
                XMM[cpu_reg].w[3] *= XMM[cpu_rm].w[3];
                XMM[cpu_reg].w[4] *= XMM[cpu_rm].w[4];
                XMM[cpu_reg].w[5] *= XMM[cpu_rm].w[5];
                XMM[cpu_reg].w[6] *= XMM[cpu_rm].w[6];
                XMM[cpu_reg].w[7] *= XMM[cpu_rm].w[7];
                CLOCK_CYCLES(1);
        }
        else
        {
                SSE_REG src;

                SEG_CHECK_READ(cpu_state.ea_seg);
                src.q[0] = readmemq(easeg, cpu_state.eaaddr);
                src.q[1] = readmemq(easeg, cpu_state.eaaddr + 8); if (cpu_state.abrt) return 0;
                XMM[cpu_reg].w[0] *= src.w[0];
                XMM[cpu_reg].w[1] *= src.w[1];
                XMM[cpu_reg].w[2] *= src.w[2];
                XMM[cpu_reg].w[3] *= src.w[3];
                XMM[cpu_reg].w[4] *= src.w[4];
                XMM[cpu_reg].w[5] *= src.w[5];
                XMM[cpu_reg].w[6] *= src.w[6];
                XMM[cpu_reg].w[7] *= src.w[7];
                CLOCK_CYCLES(2);
        }
        return 0;
}

static int opPSUBUSB_xmm_a16(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_16(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].b[0] = USATB(XMM[cpu_reg].b[0] - src.b[0]);
        XMM[cpu_reg].b[1] = USATB(XMM[cpu_reg].b[1] - src.b[1]);
        XMM[cpu_reg].b[2] = USATB(XMM[cpu_reg].b[2] - src.b[2]);
        XMM[cpu_reg].b[3] = USATB(XMM[cpu_reg].b[3] - src.b[3]);
        XMM[cpu_reg].b[4] = USATB(XMM[cpu_reg].b[4] - src.b[4]);
        XMM[cpu_reg].b[5] = USATB(XMM[cpu_reg].b[5] - src.b[5]);
        XMM[cpu_reg].b[6] = USATB(XMM[cpu_reg].b[6] - src.b[6]);
        XMM[cpu_reg].b[7] = USATB(XMM[cpu_reg].b[7] - src.b[7]);
        XMM[cpu_reg].b[8] = USATB(XMM[cpu_reg].b[8] - src.b[8]);
        XMM[cpu_reg].b[9] = USATB(XMM[cpu_reg].b[9] - src.b[9]);
        XMM[cpu_reg].b[10] = USATB(XMM[cpu_reg].b[10] - src.b[10]);
        XMM[cpu_reg].b[11] = USATB(XMM[cpu_reg].b[11] - src.b[11]);
        XMM[cpu_reg].b[12] = USATB(XMM[cpu_reg].b[12] - src.b[12]);
        XMM[cpu_reg].b[13] = USATB(XMM[cpu_reg].b[13] - src.b[13]);
        XMM[cpu_reg].b[14] = USATB(XMM[cpu_reg].b[14] - src.b[14]);
        XMM[cpu_reg].b[15] = USATB(XMM[cpu_reg].b[15] - src.b[15]);

        return 0;
}

static int opPSUBUSB_xmm_a32(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_32(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].b[0] = USATB(XMM[cpu_reg].b[0] - src.b[0]);
        XMM[cpu_reg].b[1] = USATB(XMM[cpu_reg].b[1] - src.b[1]);
        XMM[cpu_reg].b[2] = USATB(XMM[cpu_reg].b[2] - src.b[2]);
        XMM[cpu_reg].b[3] = USATB(XMM[cpu_reg].b[3] - src.b[3]);
        XMM[cpu_reg].b[4] = USATB(XMM[cpu_reg].b[4] - src.b[4]);
        XMM[cpu_reg].b[5] = USATB(XMM[cpu_reg].b[5] - src.b[5]);
        XMM[cpu_reg].b[6] = USATB(XMM[cpu_reg].b[6] - src.b[6]);
        XMM[cpu_reg].b[7] = USATB(XMM[cpu_reg].b[7] - src.b[7]);
        XMM[cpu_reg].b[8] = USATB(XMM[cpu_reg].b[8] - src.b[8]);
        XMM[cpu_reg].b[9] = USATB(XMM[cpu_reg].b[9] - src.b[9]);
        XMM[cpu_reg].b[10] = USATB(XMM[cpu_reg].b[10] - src.b[10]);
        XMM[cpu_reg].b[11] = USATB(XMM[cpu_reg].b[11] - src.b[11]);
        XMM[cpu_reg].b[12] = USATB(XMM[cpu_reg].b[12] - src.b[12]);
        XMM[cpu_reg].b[13] = USATB(XMM[cpu_reg].b[13] - src.b[13]);
        XMM[cpu_reg].b[14] = USATB(XMM[cpu_reg].b[14] - src.b[14]);
        XMM[cpu_reg].b[15] = USATB(XMM[cpu_reg].b[15] - src.b[15]);

        return 0;
}

static int opPSUBUSW_xmm_a16(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_16(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].w[0] = USATB(XMM[cpu_reg].w[0] - src.w[0]);
        XMM[cpu_reg].w[1] = USATB(XMM[cpu_reg].w[1] - src.w[1]);
        XMM[cpu_reg].w[2] = USATB(XMM[cpu_reg].w[2] - src.w[2]);
        XMM[cpu_reg].w[3] = USATB(XMM[cpu_reg].w[3] - src.w[3]);
        XMM[cpu_reg].w[4] = USATB(XMM[cpu_reg].w[4] - src.w[4]);
        XMM[cpu_reg].w[5] = USATB(XMM[cpu_reg].w[5] - src.w[5]);
        XMM[cpu_reg].w[6] = USATB(XMM[cpu_reg].w[6] - src.w[6]);
        XMM[cpu_reg].w[7] = USATB(XMM[cpu_reg].w[7] - src.w[7]);

        return 0;
}

static int opPSUBUSW_xmm_a32(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_32(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].w[0] = USATB(XMM[cpu_reg].w[0] - src.w[0]);
        XMM[cpu_reg].w[1] = USATB(XMM[cpu_reg].w[1] - src.w[1]);
        XMM[cpu_reg].w[2] = USATB(XMM[cpu_reg].w[2] - src.w[2]);
        XMM[cpu_reg].w[3] = USATB(XMM[cpu_reg].w[3] - src.w[3]);
        XMM[cpu_reg].w[4] = USATB(XMM[cpu_reg].w[4] - src.w[4]);
        XMM[cpu_reg].w[5] = USATB(XMM[cpu_reg].w[5] - src.w[5]);
        XMM[cpu_reg].w[6] = USATB(XMM[cpu_reg].w[6] - src.w[6]);
        XMM[cpu_reg].w[7] = USATB(XMM[cpu_reg].w[7] - src.w[7]);

        return 0;
}

static int opPADDUSB_xmm_a16(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_16(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].b[0] = USATB(XMM[cpu_reg].b[0] + src.b[0]);
        XMM[cpu_reg].b[1] = USATB(XMM[cpu_reg].b[1] + src.b[1]);
        XMM[cpu_reg].b[2] = USATB(XMM[cpu_reg].b[2] + src.b[2]);
        XMM[cpu_reg].b[3] = USATB(XMM[cpu_reg].b[3] + src.b[3]);
        XMM[cpu_reg].b[4] = USATB(XMM[cpu_reg].b[4] + src.b[4]);
        XMM[cpu_reg].b[5] = USATB(XMM[cpu_reg].b[5] + src.b[5]);
        XMM[cpu_reg].b[6] = USATB(XMM[cpu_reg].b[6] + src.b[6]);
        XMM[cpu_reg].b[7] = USATB(XMM[cpu_reg].b[7] + src.b[7]);
        XMM[cpu_reg].b[8] = USATB(XMM[cpu_reg].b[8] + src.b[8]);
        XMM[cpu_reg].b[9] = USATB(XMM[cpu_reg].b[9] + src.b[9]);
        XMM[cpu_reg].b[10] = USATB(XMM[cpu_reg].b[10] + src.b[10]);
        XMM[cpu_reg].b[11] = USATB(XMM[cpu_reg].b[11] + src.b[11]);
        XMM[cpu_reg].b[12] = USATB(XMM[cpu_reg].b[12] + src.b[12]);
        XMM[cpu_reg].b[13] = USATB(XMM[cpu_reg].b[13] + src.b[13]);
        XMM[cpu_reg].b[14] = USATB(XMM[cpu_reg].b[14] + src.b[14]);
        XMM[cpu_reg].b[15] = USATB(XMM[cpu_reg].b[15] + src.b[15]);

        return 0;
}

static int opPADDUSB_xmm_a32(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_32(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].b[0] = USATB(XMM[cpu_reg].b[0] + src.b[0]);
        XMM[cpu_reg].b[1] = USATB(XMM[cpu_reg].b[1] + src.b[1]);
        XMM[cpu_reg].b[2] = USATB(XMM[cpu_reg].b[2] + src.b[2]);
        XMM[cpu_reg].b[3] = USATB(XMM[cpu_reg].b[3] + src.b[3]);
        XMM[cpu_reg].b[4] = USATB(XMM[cpu_reg].b[4] + src.b[4]);
        XMM[cpu_reg].b[5] = USATB(XMM[cpu_reg].b[5] + src.b[5]);
        XMM[cpu_reg].b[6] = USATB(XMM[cpu_reg].b[6] + src.b[6]);
        XMM[cpu_reg].b[7] = USATB(XMM[cpu_reg].b[7] + src.b[7]);
        XMM[cpu_reg].b[8] = USATB(XMM[cpu_reg].b[8] + src.b[8]);
        XMM[cpu_reg].b[9] = USATB(XMM[cpu_reg].b[9] + src.b[9]);
        XMM[cpu_reg].b[10] = USATB(XMM[cpu_reg].b[10] + src.b[10]);
        XMM[cpu_reg].b[11] = USATB(XMM[cpu_reg].b[11] + src.b[11]);
        XMM[cpu_reg].b[12] = USATB(XMM[cpu_reg].b[12] + src.b[12]);
        XMM[cpu_reg].b[13] = USATB(XMM[cpu_reg].b[13] + src.b[13]);
        XMM[cpu_reg].b[14] = USATB(XMM[cpu_reg].b[14] + src.b[14]);
        XMM[cpu_reg].b[15] = USATB(XMM[cpu_reg].b[15] + src.b[15]);
        return 0;
}

static int opPADDUSW_xmm_a16(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_16(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].w[0] = USATB(XMM[cpu_reg].w[0] + src.w[0]);
        XMM[cpu_reg].w[1] = USATB(XMM[cpu_reg].w[1] + src.w[1]);
        XMM[cpu_reg].w[2] = USATB(XMM[cpu_reg].w[2] + src.w[2]);
        XMM[cpu_reg].w[3] = USATB(XMM[cpu_reg].w[3] + src.w[3]);
        XMM[cpu_reg].w[4] = USATB(XMM[cpu_reg].w[4] + src.w[4]);
        XMM[cpu_reg].w[5] = USATB(XMM[cpu_reg].w[5] + src.w[5]);
        XMM[cpu_reg].w[6] = USATB(XMM[cpu_reg].w[6] + src.w[6]);
        XMM[cpu_reg].w[7] = USATB(XMM[cpu_reg].w[7] + src.w[7]);

        return 0;
}

static int opPADDUSW_xmm_a32(uint32_t fetchdat)
{
        SSE_REG src;

        fetch_ea_32(fetchdat);
        SSE_GETSRC();

        XMM[cpu_reg].w[0] = USATB(XMM[cpu_reg].w[0] + src.w[0]);
        XMM[cpu_reg].w[1] = USATB(XMM[cpu_reg].w[1] + src.w[1]);
        XMM[cpu_reg].w[2] = USATB(XMM[cpu_reg].w[2] + src.w[2]);
        XMM[cpu_reg].w[3] = USATB(XMM[cpu_reg].w[3] + src.w[3]);
        XMM[cpu_reg].w[4] = USATB(XMM[cpu_reg].w[4] + src.w[4]);
        XMM[cpu_reg].w[5] = USATB(XMM[cpu_reg].w[5] + src.w[5]);
        XMM[cpu_reg].w[6] = USATB(XMM[cpu_reg].w[6] + src.w[6]);
        XMM[cpu_reg].w[7] = USATB(XMM[cpu_reg].w[7] + src.w[7]);

        return 0;
}