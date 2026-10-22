/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CODEGEN_TIMING_SIMD_H
#define CODEGEN_TIMING_SIMD_H

/* Operand roles are independent of ModR/M addressing and the integer tables.
 * A scalar register move reads its destination to preserve the upper lanes. */
enum {
    SIMD_R_REG = 1, SIMD_R_RM = 2, SIMD_W_REG = 4, SIMD_W_RM = 8,
    SIMD_GPR_REG = 16, SIMD_GPR_RM = 32, SIMD_MMX_REG = 64, SIMD_MMX_RM = 128
};
enum { SIMD_FP, SIMD_MUL, SIMD_DIV, SIMD_SHUFFLE, SIMD_MOVE, SIMD_LOGIC };
typedef struct simd_timing_t {
    int latency, throughput, pipe, roles, memory_cycles, decode_cycles;
} simd_timing_t;

/* Agner Fog, Instruction tables, Pentium II/III, pp. 187-188 (2025-09-20).
 * Register latency and reciprocal throughput, in core clocks. Memory is an
 * L1-hit approximation, scheduled separately from the arithmetic operation. */
static int
simd_timing_p3(uint8_t op, uint8_t prefix, int mod3, simd_timing_t *t)
{
    int scalar = prefix == 0xf3;
    if (prefix && !scalar)
        return 0;
    *t = (simd_timing_t){3, scalar ? 1 : 2, SIMD_FP,
        SIMD_R_REG | SIMD_R_RM | SIMD_W_REG, scalar ? 1 : 2, scalar ? 1 : 2};
    switch (op) {
        case 0x58: case 0x5c: case 0x5d: case 0x5f: case 0xc2:
            return 1;
        case 0x59:
            t->latency=4; t->pipe=SIMD_MUL; return 1;
        case 0x5e:
            t->latency=scalar ? 18 : 48; t->throughput=scalar ? 17 : 34;
            t->pipe=SIMD_DIV; return 1;
        case 0x51:
            t->latency=scalar ? 30 : 56; t->throughput=scalar ? 28 : 56;
            t->pipe=SIMD_DIV; t->roles=SIMD_R_RM|SIMD_W_REG; return 1;
        case 0x52: case 0x53:
            t->latency=scalar ? 1 : 2; t->roles=SIMD_R_RM|SIMD_W_REG; return 1;
        case 0x54: case 0x55: case 0x56: case 0x57:
            if (scalar) return 0;
            t->latency=2; t->pipe=SIMD_LOGIC; return 1;
        case 0x10: case 0x11: case 0x28: case 0x29: case 0x2b:
            if (scalar && op != 0x10 && op != 0x11) return 0;
            t->pipe=SIMD_MOVE; t->latency=1; t->throughput=1; t->decode_cycles=1;
            t->roles=(op == 0x11 || op == 0x29 || op == 0x2b)
                ? SIMD_R_REG|SIMD_W_RM : SIMD_R_RM|SIMD_W_REG;
            if (scalar && mod3) t->roles |= op == 0x11 ? SIMD_R_RM : SIMD_R_REG;
            if (!mod3 && !scalar) t->memory_cycles=(op == 0x10 || op == 0x11) ? 4 : 2;
            return 1;
        case 0x12: case 0x13: case 0x16: case 0x17:
            if (scalar) return 0;
            t->latency=t->throughput=t->memory_cycles=t->decode_cycles=1; t->pipe=SIMD_MOVE;
            if (op&1) t->roles=SIMD_R_REG|SIMD_W_RM;
            return 1;
        case 0x14: case 0x15: case 0xc6:
            if (scalar) return 0;
            t->latency=op == 0xc6 ? 2 : 3; t->pipe=SIMD_SHUFFLE; return 1;
        case 0x50:
            if (scalar) return 0;
            t->latency=t->throughput=t->decode_cycles=1; t->pipe=SIMD_MOVE;
            t->roles=SIMD_R_RM|SIMD_W_REG|SIMD_GPR_REG; return 1;
        case 0x2e: case 0x2f:
            if (scalar) return 0;
            t->latency=t->throughput=t->memory_cycles=t->decode_cycles=1;
            t->roles=SIMD_R_REG|SIMD_R_RM; return 1;
        case 0x2a:
            t->latency=scalar ? 4 : 3; t->throughput=scalar ? 2 : 1;
            t->roles=SIMD_R_REG|SIMD_R_RM|SIMD_W_REG|(scalar ? SIMD_GPR_RM : SIMD_MMX_RM);
            return 1;
        case 0x2c: case 0x2d:
            t->latency=3; t->throughput=t->decode_cycles=1;
            t->roles=SIMD_R_RM|SIMD_W_REG|(scalar ? SIMD_GPR_REG : SIMD_MMX_REG);
            return 1;
        default: return 0;
    }
}
#endif
