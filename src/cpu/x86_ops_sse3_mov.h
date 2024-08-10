/* SPDX-License-Identifier: GPL-2.0-or-later */
static int
opMOVDDUP_a16(uint32_t fetchdat)
{
    SSE_REG src;

    fetch_ea_16(fetchdat);
    SSE_GETSRC();

    XMM[cpu_reg].q[0] = src.q[0];
    XMM[cpu_reg].q[1] = src.q[0];
    return 0;
}

static int
opMOVDDUP_a32(uint32_t fetchdat)
{
    SSE_REG src;

    fetch_ea_32(fetchdat);
    SSE_GETSRC();

    XMM[cpu_reg].q[0] = src.q[0];
    XMM[cpu_reg].q[1] = src.q[0];
    return 0;
}

static int
opMOVSLDUP_a16(uint32_t fetchdat)
{
    SSE_REG src;

    fetch_ea_16(fetchdat);
    SSE_GETSRC();

    XMM[cpu_reg].l[0] = src.l[0];
    XMM[cpu_reg].l[1] = src.l[0];
    XMM[cpu_reg].l[2] = src.l[2];
    XMM[cpu_reg].l[3] = src.l[2];
    return 0;
}

static int
opMOVSLDUP_a32(uint32_t fetchdat)
{
    SSE_REG src;

    fetch_ea_32(fetchdat);
    SSE_GETSRC();

    XMM[cpu_reg].l[0] = src.l[0];
    XMM[cpu_reg].l[1] = src.l[0];
    XMM[cpu_reg].l[2] = src.l[2];
    XMM[cpu_reg].l[3] = src.l[2];
    return 0;
}

static int
opMOVSHDUP_a16(uint32_t fetchdat)
{
    SSE_REG src;

    fetch_ea_16(fetchdat);
    SSE_GETSRC();

    XMM[cpu_reg].l[0] = src.l[1];
    XMM[cpu_reg].l[1] = src.l[1];
    XMM[cpu_reg].l[2] = src.l[3];
    XMM[cpu_reg].l[3] = src.l[3];
    return 0;
}

static int
opMOVSHDUP_a32(uint32_t fetchdat)
{
    SSE_REG src;

    fetch_ea_32(fetchdat);
    SSE_GETSRC();

    XMM[cpu_reg].l[0] = src.l[1];
    XMM[cpu_reg].l[1] = src.l[1];
    XMM[cpu_reg].l[2] = src.l[3];
    XMM[cpu_reg].l[3] = src.l[3];
    return 0;
}