/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Willamette timing, in half core clocks. Intel 248966-004, Appendix C:
 * https://kib.kiev.ua/x86docs/Intel/Intel-OptimGuide/248966-004.pdf
 * This is a warm trace-cache/L1 scheduling model, not a cache, predictor or
 * replay simulator. Unlisted integer/system operations use a four-clock
 * estimate. Execution resources model the clusters in Figure 1-4. */
#include <stdint.h>
#include <string.h>
#include <86box/86box.h>
#include "cpu.h"
#include "codegen.h"
#include "codegen_timing_common.h"
#include "codegen_timing_simd.h"

enum { NB_ALU,
       NB_SHIFT,
       NB_ADD,
       NB_MUL,
       NB_DIV,
       NB_SHUFFLE,
       NB_MOVE,
       NB_LOGIC,
       NB_LOAD,
       NB_STORE,
       NB_UNITS };
static struct {
    int gpr[8], xmm[8], mmx[8], st[8], flags;
    int unit[NB_UNITS], decode, slots, complete, charged;
} nb;
static uint8_t nb_escape, nb_prefix;
static int
nb_max(int a, int b)
{
    return a > b ? a : b;
}

static int
nb_issue(int uops)
{
    int start = nb.decode;
    nb.slots += uops;
    nb.decode += (nb.slots / 3) * 2; /* Trace cache delivers up to three uops/clock. */
    nb.slots %= 3;
    return start;
}
static int
nb_execute(int unit, int start, int latency, int throughput)
{
    start         = nb_max(start, nb.unit[unit]);
    nb.unit[unit] = start + throughput;
    return start + latency;
}
static void
nb_charge(int complete)
{
    nb.complete = nb_max(nb.complete, complete);
    int clocks  = (nb.complete + 1) / 2;
    codegen_block_cycles += clocks - nb.charged;
    nb.charged = clocks;
}
static int
nb_address(uint32_t fetchdat, int op_32, int start)
{
    uint32_t mask = get_addr_regmask(MODRM, fetchdat, op_32);
    for (int i = 0; i < 8; ++i)
        if (mask & (1 << i))
            start = nb_max(start, nb.gpr[i]);
    return start;
}

/* Table C-1 through C-5, register forms. Operand roles also distinguish MMX
 * from XMM and GPR conversions; they never alias the integer register index. */
static int
nb_simd(uint8_t op, uint32_t fetchdat, simd_timing_t *t)
{
    int mod3   = (fetchdat & 0xc0) == 0xc0;
    int scalar = nb_prefix == 0xf3 || nb_prefix == 0xf2;
    int dbl    = nb_prefix == 0x66 || nb_prefix == 0xf2;
    if (simd_timing_p3(op, scalar ? 0xf3 : 0, mod3, t)) {
        t->latency       = 4;
        t->throughput    = 2;
        t->decode_cycles = 1;
        t->memory_cycles = 6;
        switch (op) {
            case 0x59:
                t->latency = 6;
                break;
            case 0x51:
            case 0x5e:
                t->latency    = dbl ? (scalar ? 35 : 62) : (scalar ? 22 : 32);
                t->throughput = t->latency;
                break;
            case 0x52:
            case 0x53:
                t->latency    = 6;
                t->throughput = (scalar && op == 0x53) ? 2 : 4;
                break;
            case 0x10:
            case 0x11:
            case 0x28:
            case 0x29:
            case 0x2b:
                t->latency    = scalar && !dbl ? 4 : 6;
                t->throughput = scalar ? 2 : 1;
                break;
            case 0x12:
            case 0x16:
            case 0x14:
            case 0x15:
            case 0xc6:
                t->latency = (op == 0x16 || op == 0x14) ? 4 : 6;
                break;
            case 0x50:
            case 0x2e:
            case 0x2f:
                t->latency = 6;
                break;
            case 0x2a:
                t->latency    = scalar ? (dbl ? 15 : 11) : 11;
                t->throughput = scalar ? (dbl ? 3 : 2) : 4;
                if (dbl && !scalar)
                    t->roles &= ~SIMD_R_REG;
                break;
            case 0x2c:
            case 0x2d:
                t->latency    = scalar ? 8 : (dbl ? 11 : 7);
                t->throughput = !scalar && dbl ? 3 : 2;
                break;
        }
        return 1;
    }
    *t = (simd_timing_t) { 2, nb_prefix == 0x66 ? 2 : 1, SIMD_LOGIC,
                           SIMD_R_REG | SIMD_R_RM | SIMD_W_REG, 6, 1 };
    if (!nb_prefix)
        t->roles |= SIMD_MMX_REG | SIMD_MMX_RM;
    if (op == 0x5a || op == 0x5b || (op == 0xe6 && nb_prefix)) {
        t->pipe       = SIMD_FP;
        t->roles      = SIMD_R_RM | SIMD_W_REG;
        t->throughput = 2;
        if (op == 0x5b)
            t->latency = 5;
        else if (op == 0xe6) {
            t->latency    = nb_prefix == 0xf3 ? 8 : 9;
            t->throughput = nb_prefix == 0xf3 ? 3 : 2;
        } else {
            t->latency    = nb_prefix == 0xf2 ? 16 : nb_prefix == 0xf3 ? 14
                                                                       : 10;
            t->throughput = nb_prefix == 0xf2 || !nb_prefix ? 4 : nb_prefix == 0xf3 ? 3
                                                                                    : 2;
            if (scalar)
                t->roles |= SIMD_R_REG;
        }
        return 1;
    }
    switch (op) {
        case 0x6f:
        case 0x7f:
        case 0xe7:
            t->latency    = 6;
            t->throughput = 1;
            t->pipe       = SIMD_MOVE;
            t->roles &= ~(SIMD_R_REG | SIMD_R_RM | SIMD_W_REG);
            t->roles |= op == 0x6f ? SIMD_R_RM | SIMD_W_REG : SIMD_R_REG | SIMD_W_RM;
            return nb_prefix == 0 || nb_prefix == 0x66 || nb_prefix == 0xf3;
        case 0x6e:
        case 0x7e:
            if (nb_prefix == 0xf3 && op == 0x7e) {
                t->roles      = SIMD_R_RM | SIMD_W_REG;
                t->throughput = 2;
                return 1;
            }
            t->roles = op == 0x6e ? SIMD_R_RM | SIMD_W_REG | SIMD_GPR_RM : SIMD_R_REG | SIMD_W_RM | SIMD_GPR_RM;
            if (!nb_prefix)
                t->roles |= SIMD_MMX_REG;
            t->latency = op == 0x6e ? (nb_prefix ? 6 : 2) : (nb_prefix ? 10 : 5);
            return nb_prefix == 0 || nb_prefix == 0x66;
        case 0x70:
            t->pipe = SIMD_SHUFFLE;
            t->roles &= ~SIMD_R_REG;
            t->latency = nb_prefix == 0x66 ? 4 : 2;
            return 1;
        case 0x71:
        case 0x72:
        case 0x73:
            t->pipe = SIMD_SHUFFLE;
            t->roles &= ~(SIMD_R_REG | SIMD_W_REG);
            t->roles |= SIMD_W_RM;
            if (op == 0x73 && ((fetchdat & 0x38) == 0x18 || (fetchdat & 0x38) == 0x38))
                t->latency = 4;
            return nb_prefix == 0 || nb_prefix == 0x66;
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6a:
        case 0x6b:
        case 0x6c:
        case 0x6d:
            t->pipe    = SIMD_SHUFFLE;
            t->latency = nb_prefix == 0x66 ? 4 : 2;
            if (op <= 0x62)
                t->latency = 2;
            return nb_prefix == 0 || nb_prefix == 0x66;
        case 0xd1:
        case 0xd2:
        case 0xd3:
        case 0xe1:
        case 0xe2:
        case 0xf1:
        case 0xf2:
        case 0xf3:
            t->pipe = SIMD_SHUFFLE;
            return nb_prefix == 0 || nb_prefix == 0x66;
        case 0xd5:
        case 0xe4:
        case 0xe5:
        case 0xf4:
        case 0xf5:
            t->pipe    = SIMD_MUL;
            t->latency = 8;
            return nb_prefix == 0 || nb_prefix == 0x66;
        case 0xd4:
        case 0xfb:
            t->latency = nb_prefix == 0x66 ? 6 : 2;
            return nb_prefix == 0 || nb_prefix == 0x66;
        case 0xf6:
            t->latency = 4;
            return nb_prefix == 0 || nb_prefix == 0x66;
        case 0x64:
        case 0x65:
        case 0x66:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0xd8:
        case 0xd9:
        case 0xda:
        case 0xdb:
        case 0xdc:
        case 0xdd:
        case 0xde:
        case 0xdf:
        case 0xe0:
        case 0xe3:
        case 0xe8:
        case 0xe9:
        case 0xea:
        case 0xeb:
        case 0xec:
        case 0xed:
        case 0xee:
        case 0xef:
        case 0xf8:
        case 0xf9:
        case 0xfa:
        case 0xfc:
        case 0xfd:
        case 0xfe:
            return nb_prefix == 0 || nb_prefix == 0x66;
        default:
            return 0;
    }
}
static void
nb_schedule_simd(const simd_timing_t *t, uint32_t fetchdat, int op_32)
{
    static const int pipes[] = { NB_ADD, NB_MUL, NB_DIV, NB_SHUFFLE, NB_MOVE, NB_LOGIC };
    int              reg = (fetchdat >> 3) & 7, rm = fetchdat & 7, mod3 = (fetchdat & 0xc0) == 0xc0;
    int             *r     = (t->roles & SIMD_GPR_REG) ? nb.gpr : (t->roles & SIMD_MMX_REG) ? nb.mmx
                                                                                            : nb.xmm;
    int             *m     = (t->roles & SIMD_GPR_RM) ? nb.gpr : (t->roles & SIMD_MMX_RM) ? nb.mmx
                                                                                          : nb.xmm;
    int              start = nb_issue(mod3 ? 1 : 2), addr = nb_address(fetchdat, op_32, start);
    if (t->roles & SIMD_R_REG)
        start = nb_max(start, r[reg]);
    if (mod3 && (t->roles & SIMD_R_RM))
        start = nb_max(start, m[rm]);
    if (!mod3 && (t->roles & SIMD_R_RM))
        start = nb_max(start, nb_execute(NB_LOAD, addr, 12, 2));
    int complete;
    if (!mod3 && (t->roles & SIMD_W_RM))
        complete = nb_execute(NB_STORE, nb_max(start, addr), 2, 2);
    else {
        complete = nb_execute(pipes[t->pipe], start, t->latency * 2, t->throughput * 2);
        if (t->roles & SIMD_W_REG)
            r[reg] = complete;
        if (t->roles & SIMD_W_RM)
            m[rm] = complete;
        if (!(t->roles & (SIMD_W_REG | SIMD_W_RM)))
            nb.flags = complete;
    }
    nb_charge(complete);
}
static void
nb_start(void)
{
    nb_escape = nb_prefix = 0;
}
static void
nb_prefix_op(uint8_t op, uint32_t fetchdat)
{
    (void) fetchdat;
    if (op == 0x0f || (op >= 0xd8 && op <= 0xdf))
        nb_escape = op;
    if (op == 0x66 || op == 0xf2 || op == 0xf3)
        nb_prefix = op;
}
static void
nb_block_start(void)
{
    memset(&nb, 0, sizeof(nb));
    nb_start();
}
static void
nb_block_end(void)
{
    nb_charge(nb.complete);
}
static int
nb_jump_cycles(void)
{
    return 1;
}

static uint64_t
nb_dependencies(uint8_t op, uint32_t f, int mod3)
{
    unsigned group = (f >> 3) & 7;
    if (nb_escape == 0x0f) {
        /* The shared 0F tables only describe instruction lengths for these
         * opcodes. Supply the actual register roles for scheduling. */
        if (op == 0xaf || (op >= 0x40 && op <= 0x4f))
            return MODRM | SRCDEP_REG | DSTDEP_REG | (mod3 ? SRCDEP_RM : 0);
        if (op == 0xb6 || op == 0xb7 || op == 0xbe || op == 0xbf)
            return MODRM | DSTDEP_REG | (mod3 ? SRCDEP_RM : 0);
        if ((op & 0xf0) == 0x90)
            return MODRM | (mod3 ? DSTDEP_RM : 0);
        return (mod3 ? opcode_deps_0f_mod3 : opcode_deps_0f)[op];
    }
    switch (op) {
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
            return (mod3 ? opcode_deps_8x_mod3 : opcode_deps_8x)[group];
        case 0xc0:
        case 0xc1:
        case 0xd0:
        case 0xd1:
            return (mod3 ? opcode_deps_shift_mod3 : opcode_deps_shift)[group];
        case 0xd2:
        case 0xd3:
            return (mod3 ? opcode_deps_shift_cl_mod3 : opcode_deps_shift_cl)[group];
        case 0xf6:
            return (mod3 ? opcode_deps_f6_mod3 : opcode_deps_f6)[group];
        case 0xf7:
            return (mod3 ? opcode_deps_f7_mod3 : opcode_deps_f7)[group];
        case 0xff:
            return (mod3 ? opcode_deps_ff_mod3 : opcode_deps_ff)[group];
        default:
            return (mod3 ? opcode_deps_mod3 : opcode_deps)[op];
    }
}
static void
nb_x87(uint8_t op, uint32_t fetchdat, int op_32)
{
    int             mod3 = (fetchdat & 0xc0) == 0xc0, group = (op >> 3) & 7, index = op & 7;
    const uint64_t *table = NULL;
    switch (nb_escape) {
        case 0xd8:
            table = mod3 ? opcode_deps_d8_mod3 : opcode_deps_d8;
            break;
        case 0xd9:
            table = mod3 ? opcode_deps_d9_mod3 : opcode_deps_d9;
            if (mod3)
                group = op & 0x3f;
            break;
        case 0xda:
            table = mod3 ? opcode_deps_da_mod3 : opcode_deps_da;
            break;
        case 0xdb:
            table = mod3 ? opcode_deps_db_mod3 : opcode_deps_db;
            if (mod3)
                group = op & 0x3f;
            break;
        case 0xdc:
            table = mod3 ? opcode_deps_dc_mod3 : opcode_deps_dc;
            break;
        case 0xdd:
            table = mod3 ? opcode_deps_dd_mod3 : opcode_deps_dd;
            break;
        case 0xde:
            table = mod3 ? opcode_deps_de_mod3 : opcode_deps_de;
            break;
        case 0xdf:
            table = mod3 ? opcode_deps_df_mod3 : opcode_deps_df;
            break;
    }
    uint64_t deps  = table[group];
    int      start = nb_issue(1), latency = 8, throughput = 2, unit = NB_MOVE;
    if (deps & FPU_READ_ST0)
        start = nb_max(start, nb.st[0]);
    if (deps & FPU_READ_ST1)
        start = nb_max(start, nb.st[1]);
    if (deps & FPU_READ_STREG)
        start = nb_max(start, nb.st[index]);
    if (!mod3)
        start = nb_max(start, nb_execute(NB_LOAD, nb_address(fetchdat, op_32, start), 12, 2));
    if (nb_escape == 0xd8 || nb_escape == 0xdc || nb_escape == 0xde) {
        switch ((op >> 3) & 7) {
            case 0:
            case 4:
            case 5:
                latency    = 10;
                throughput = 2;
                unit       = NB_ADD;
                break;
            case 1:
                latency    = 14;
                throughput = 4;
                unit       = NB_MUL;
                break;
            case 2:
            case 3:
                latency    = 4;
                throughput = 2;
                break;
            case 6:
            case 7:
                latency = throughput = ((cpu_state.npxc & 0x300) == 0 ? 23 : (cpu_state.npxc & 0x300) == 0x200 ? 38
                                                                                                               : 43)
                    * 2;
                unit = NB_DIV;
                break;
        }
    } else if (nb_escape == 0xd9 && op == 0xfa) {
        latency = throughput = ((cpu_state.npxc & 0x300) == 0 ? 23 : (cpu_state.npxc & 0x300) == 0x200 ? 38
                                                                                                       : 43)
            * 2;
        unit = NB_DIV;
    } else if (nb_escape == 0xd9 && (op & 0xf8) == 0xc8) {
        int ready    = nb.st[0];
        nb.st[0]     = nb.st[index];
        nb.st[index] = ready;
        nb_charge(nb_execute(NB_MOVE, nb.decode, 0, 2));
        return;
    }
    int complete = nb_execute(unit, start, latency, throughput);
    if (deps & FPU_PUSH) {
        for (int i = 7; i > 0; --i)
            nb.st[i] = nb.st[i - 1];
        nb.st[0] = 0;
    }
    if (deps & FPU_WRITE_ST0)
        nb.st[0] = complete;
    if (deps & FPU_WRITE_ST1)
        nb.st[1] = complete;
    if (deps & FPU_WRITE_STREG)
        nb.st[index] = complete;
    int pops = (deps & FPU_POP2) ? 2 : (deps & FPU_POP) ? 1
                                                        : 0;
    if (pops) {
        for (int i = 0; i < 8 - pops; ++i)
            nb.st[i] = nb.st[i + pops];
        for (int i = 8 - pops; i < 8; ++i)
            nb.st[i] = 0;
    }
    nb_charge(complete);
}
static void
nb_opcode(uint8_t op, uint32_t fetchdat, int op_32, uint32_t op_pc)
{
    (void) op_pc;
    int mod3 = (fetchdat & 0xc0) == 0xc0, group = (fetchdat >> 3) & 7;
    /* Preserve the existing cache-flush estimate and serialize the scheduler.
     * Its actual cost depends on cache residency, which is not modeled here. */
    if (nb_escape == 0x0f && (op == 0x08 || op == 0x09)) {
        int complete = nb.complete + (op == 0x09 ? 20000 : 2000);
        for (int i = 0; i < NB_UNITS; ++i)
            nb.unit[i] = complete;
        nb.decode = complete;
        nb.slots  = 0;
        nb_charge(complete);
        return;
    }
    simd_timing_t simd;
    if (nb_escape == 0x0f && nb_simd(op, fetchdat, &simd)) {
        nb_schedule_simd(&simd, fetchdat, op_32);
        return;
    }
    if (nb_escape >= 0xd8 && nb_escape <= 0xdf) {
        nb_x87(op, fetchdat, op_32);
        return;
    }
    uint64_t deps = nb_dependencies(op, fetchdat, mod3);
    int      byte = nb_escape == 0x0f ? (op & 0xf0) == 0x90 : !(op & 1);
    uint32_t src = get_srcdep_mask(deps, fetchdat, byte, op_32), dst = get_dstdep_mask(deps, fetchdat, byte);
    if (src & REGMASK_IMPL_ESP)
        src |= 1 << 4;
    int latency = 8, throughput = 2, unit = NB_ALU, read_flags = 0, write_flags = 0;
    if (!nb_escape) {
        if (op < 0x40 && (op & 7) < 6) {
            latency = throughput = 1;
            write_flags          = 1;
            if ((op & 0x38) == 0x10 || (op & 0x38) == 0x18) {
                latency    = (op & 7) < 4 ? 16 : 12;
                throughput = (op & 7) < 4 ? 6 : 4;
                read_flags = 1;
            }
        } else if (op >= 0x80 && op <= 0x83) {
            latency = throughput = 1;
            write_flags          = 1;
            if (group == 2 || group == 3) {
                latency    = 12;
                throughput = 4;
                read_flags = 1;
            }
        } else if ((op >= 0x88 && op <= 0x8b) || (op >= 0xb0 && op <= 0xbf) || op == 0x90 || op == 0x84 || op == 0x85) {
            latency = throughput = 1;
            write_flags          = op == 0x84 || op == 0x85;
        } else if (op >= 0x40 && op <= 0x4f) {
            latency     = 2;
            throughput  = 1;
            write_flags = 1;
        } else if (op == 0x69 || op == 0x6b) {
            latency     = 28;
            throughput  = 6;
            unit        = NB_MUL;
            write_flags = 1;
        } else if (op == 0xc0 || op == 0xc1 || (op >= 0xd0 && op <= 0xd3)) {
            latency     = 8;
            throughput  = 2;
            unit        = NB_SHIFT;
            write_flags = 1;
        } else if (op == 0xf6 || op == 0xf7) {
            if (group >= 6) {
                latency    = 140;
                throughput = 46;
                unit       = NB_DIV;
            } else if (group >= 4) {
                latency    = 36;
                throughput = 10;
                unit       = NB_MUL;
            } else {
                latency = throughput = 1;
                write_flags          = group != 2;
            }
        } else if ((op & 0xf0) == 0x70) {
            latency    = 0;
            throughput = 1;
            read_flags = 1;
        } else if (op == 0xe9 || op == 0xeb) {
            latency    = 0;
            throughput = 1;
        } else if (op >= 0xe0 && op <= 0xe2) {
            latency    = 16;
            throughput = 3;
            src |= 2;
            dst |= 2;
            read_flags = op != 0xe2;
        } else if (op == 0xe8) {
            latency    = 10;
            throughput = 2;
        } else if (op == 0xc2 || op == 0xc3) {
            latency    = 16;
            throughput = 2;
        }
    } else {
        if (op == 0xaf) {
            latency     = 28;
            throughput  = 6;
            unit        = NB_MUL;
            write_flags = 1;
        } else if (op == 0xb6 || op == 0xb7 || op == 0xbe || op == 0xbf)
            latency = throughput = 1;
        else if ((op & 0xf0) == 0x80) {
            latency    = 0;
            throughput = 1;
            read_flags = 1;
        } else if ((op & 0xf0) == 0x90) {
            latency    = 10;
            throughput = 3;
            read_flags = 1;
        }
    }
    int memory = !mod3 && (deps & MODRM), start = nb_issue(memory ? 2 : 1);
    for (int i = 0; i < 8; ++i)
        if (src & (1 << i))
            start = nb_max(start, nb.gpr[i]);
    if (read_flags)
        start = nb_max(start, nb.flags);
    if (memory && op != 0x8d) {
        /* The dependency tables encode memory reads without an RM register.
         * Pure stores are identified explicitly; remaining forms may read. */
        int store_only = !nb_escape && (op == 0x88 || op == 0x89 || op == 0xc6 || op == 0xc7);
        if (!store_only)
            start = nb_max(start, nb_execute(NB_LOAD, nb_address(fetchdat, op_32, start), 4, 2));
    }
    int complete = nb_execute(unit, start, latency, throughput);
    for (int i = 0; i < 8; ++i)
        if (dst & (1 << i))
            nb.gpr[i] = complete;
    if (write_flags)
        nb.flags = complete;
    if (memory && !nb_escape && (op == 0x88 || op == 0x89 || op == 0xc6 || op == 0xc7 || (op < 0x38 && (op & 7) < 2)))
        complete = nb_execute(NB_STORE, complete, 2, 2);
    nb_charge(complete);
}

codegen_timing_t codegen_timing_p4 = { nb_start, nb_prefix_op, nb_opcode, nb_block_start, nb_block_end, nb_jump_cycles };
