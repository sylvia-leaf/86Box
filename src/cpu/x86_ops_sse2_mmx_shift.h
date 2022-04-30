static int opPSxxW_xmm_imm(uint32_t fetchdat)
{
        int reg = fetchdat & 7;
        int op = fetchdat & 0x38;
        int shift = (fetchdat >> 8) & 0xff;

        cpu_state.pc += 2;

        switch (op)
        {
                case 0x10: /*PSRLW*/
                if (shift > 15)
                {
                        XMM[reg].q[0] = 0;
                        XMM[reg].q[1] = 0;
                }
                else
                {
                        XMM[reg].w[0] >>= shift;
                        XMM[reg].w[1] >>= shift;
                        XMM[reg].w[2] >>= shift;
                        XMM[reg].w[3] >>= shift;
                        XMM[reg].w[4] >>= shift;
                        XMM[reg].w[5] >>= shift;
                        XMM[reg].w[6] >>= shift;
                        XMM[reg].w[7] >>= shift;
                }
                break;
                case 0x20: /*PSRAW*/
                if (shift > 15)
                        shift = 15;
                XMM[reg].sw[0] >>= shift;
                XMM[reg].sw[1] >>= shift;
                XMM[reg].sw[2] >>= shift;
                XMM[reg].sw[3] >>= shift;
                XMM[reg].sw[4] >>= shift;
                XMM[reg].sw[5] >>= shift;
                XMM[reg].sw[6] >>= shift;
                XMM[reg].sw[7] >>= shift;
                break;
                case 0x30: /*PSLLW*/
                if (shift > 15)
                {
                        XMM[reg].q[0] = 0;
                        XMM[reg].q[1] = 0;
                }
                else
                {
                        XMM[reg].w[0] <<= shift;
                        XMM[reg].w[1] <<= shift;
                        XMM[reg].w[2] <<= shift;
                        XMM[reg].w[3] <<= shift;
                        XMM[reg].w[4] <<= shift;
                        XMM[reg].w[5] <<= shift;
                        XMM[reg].w[6] <<= shift;
                        XMM[reg].w[7] <<= shift;
                }
                break;
                default:
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                return 0;
        }

        CLOCK_CYCLES(1);
        return 0;
}