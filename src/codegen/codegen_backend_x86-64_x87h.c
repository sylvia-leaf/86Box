/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          The host-x87 FPU's compiled instructions, done inline by the
 *          block (UOP_X87H, UOP_X87H_DONE; codegen_ops_x87h.c builds them).
 *
 *          An instruction runs on softfloat's registers where they are: the
 *          block tests the plain case (every exception masked, none pending,
 *          precision control not the reserved 01, the registers it reads
 *          full and the one it pushes to empty) and jumps to the handler's
 *          call when it does not hold, having changed nothing. Otherwise it
 *          loads the guest's control word into the host x87 if the host's
 *          own differs, clears the host's exception flags if the guest has
 *          cleared some of them or the host holds one the guest was never
 *          given (x87_host.h), loads the operands from their slots, runs the
 *          guest's own opcode, takes the status word, puts the host's control
 *          word back (both host ABIs preserve it across calls, and the C
 *          library on this thread uses the x87) and stores the result in its
 *          slot. The tag, the stack top and the status word follow as
 *          the softfloat handler leaves them.
 *
 *          The uops are barriers: no guest register is in a host register,
 *          so the code has RAX, RCX, RDX and RSI to itself, with RDI the
 *          address of fpu_state and RBP, as everywhere in a block, that of
 *          cpu_state + 128. RSI holds the stack top throughout.
 *
 * Authors: Michael Pratte, <mpratte@makefox.group>
 *
 *          Copyright 2026 Michael Pratte.
 */

/* Included by codegen_backend_x86-64_uops.c. */

#if X87_HOST_AVAILABLE

#    define XR_AX 0
#    define XR_CX 1
#    define XR_DX 2
#    define XR_SI 6
#    define XR_DI 7
#    define XR_BP 5

#    define XFS(field) ((int32_t) offsetof(fpu_state_t, field))
#    define XCS(field)  ((int32_t) ((uintptr_t) &cpu_state.field - (uintptr_t) &cpu_state) - 128)

#    define X87H_FS_CWD XFS(cwd)
#    define X87H_FS_SWD XFS(swd)
#    define X87H_FS_TAG XFS(tag)
#    define X87H_FS_TOS XFS(tos)
#    define X87H_FS_ST  XFS(st_space)

static void
xb(uint8_t b)
{
    codegen_addbyte(NULL, b);
}

static void
xl(uint32_t l)
{
    codegen_addlong(NULL, l);
}

/* ModR/M for [base + disp32], base not RSP. */
static void
x_mem(int reg, int base, int32_t disp)
{
    xb(0x80 | (reg << 3) | base);
    xl((uint32_t) disp);
}

/* ModR/M for [base], base not RSP or RBP. */
static void
x_ind(int reg, int base)
{
    xb((reg << 3) | base);
}

/* reg32 op reg32 */
static void
x_rr(uint8_t op, int reg, int rm)
{
    xb(op);
    xb(0xc0 | (reg << 3) | rm);
}

/* A forward jump, rel32: returns where to patch. */
static uint8_t *
x_jcc(uint8_t cc)
{
    xb(0x0f);
    xb(0x80 | cc);
    xl(0);
    return &block_write_data[block_pos - 4];
}

/* A short forward jump: returns where to patch. */
static uint8_t *
x_jcc8(uint8_t cc)
{
    xb(0x70 | cc);
    xb(0);
    return &block_write_data[block_pos - 1];
}

static void
x_here8(uint8_t *p)
{
    *p = (uint8_t) (&block_write_data[block_pos] - (p + 1));
}

#    define XCC_Z  0x4
#    define XCC_NZ 0x5

/* The tests' jumps to the handler's call: each to one JMP at the end, whose
   target the IR sets (uop->p). */
typedef struct {
    uint8_t *fail[8];
    int      n;
} x87h_fails_t;

static void
x_fail(x87h_fails_t *f, uint8_t cc)
{
    if (f->n >= 8)
        fatal("x87h: too many plain-case tests\n");
    f->fail[f->n++] = x_jcc(cc);
}

/* reg = (tos + k) & 7 */
static void
x_stk(int reg, int k)
{
    xb(0x8d); /* LEA reg, [RSI + k] */
    xb(0x40 | (reg << 3) | XR_SI);
    xb((uint8_t) k);
    xb(0x83); /* AND reg, 7 */
    xb(0xe0 | reg);
    xb(7);
}

/* reg (64-bit) = the address of ST(k)'s slot. */
static void
x_slot(int reg, int k)
{
    x_stk(reg, k);
    xb(0xc1); /* SHL reg, 4 */
    xb(0xe0 | reg);
    xb(4);
    xb(0x48); /* LEA reg, [RDI + reg + st_space] */
    xb(0x8d);
    xb(0x84 | (reg << 3));
    xb((reg << 3) | XR_DI);
    xl((uint32_t) X87H_FS_ST);
}

/* Fails unless ST(k) is full (full = 1) or empty (full = 0). Uses EAX, ECX. */
static void
x_tag_test(x87h_fails_t *f, int k, int full)
{
    x_stk(XR_CX, k);
    x_rr(0x01, XR_CX, XR_CX); /* ADD ECX, ECX */
    xb(0x0f);                 /* MOVZX EAX, word [RDI + tag] */
    xb(0xb7);
    x_mem(XR_AX, XR_DI, X87H_FS_TAG);
    x_rr(0xd3, 5, XR_AX); /* SHR EAX, CL */
    xb(0x83);             /* AND EAX, 3 */
    xb(0xe0);
    xb(3);
    xb(0x83); /* CMP EAX, 3 */
    xb(0xf8);
    xb(3);
    x_fail(f, full ? XCC_Z : XCC_NZ);
}

/* ST(k)'s tag: valid (value 0) or empty (value 3). Uses EAX, ECX. */
static void
x_tag_set(int k, int empty)
{
    x_stk(XR_CX, k);
    x_rr(0x01, XR_CX, XR_CX); /* ADD ECX, ECX */
    xb(0xb8);                 /* MOV EAX, 3 */
    xl(3);
    x_rr(0xd3, 4, XR_AX); /* SHL EAX, CL */
    if (empty) {
        xb(0x66); /* OR [RDI + tag], AX */
        xb(0x09);
    } else {
        x_rr(0xf7, 2, XR_AX); /* NOT EAX */
        xb(0x66);             /* AND [RDI + tag], AX */
        xb(0x21);
    }
    x_mem(XR_AX, XR_DI, X87H_FS_TAG);
}

/* The stack top moved by d (1: pop, -1: push), in RSI and fpu_state. */
static void
x_tos_move(int d)
{
    xb(0x8d); /* LEA ESI, [RSI + d] */
    xb(0x40 | (XR_SI << 3) | XR_SI);
    xb((uint8_t) d);
    xb(0x83); /* AND ESI, 7 */
    xb(0xe0 | XR_SI);
    xb(7);
    xb(0x40); /* MOV [RDI + tos], SIL */
    xb(0x88);
    x_mem(XR_SI, XR_DI, X87H_FS_TOS);
}

/* The pop: ST(0) empty, the top moved up. */
static void
x_pop(void)
{
    x_tag_set(0, 1);
    x_tos_move(1);
}

/* C1 cleared, as a copy leaves it. */
static void
x_clear_c1(void)
{
    xb(0x66); /* AND word [RDI + swd], ~C1 */
    xb(0x81);
    x_mem(4, XR_DI, X87H_FS_SWD);
    xb(0xff);
    xb(0xfd);
}

/* RDI = &fpu_state, the plain case (exceptions masked, none pending,
   precision control not 01) tested, RSI = the stack top. */
static void
x_enter(x87h_fails_t *f)
{
    xb(0x48); /* MOV RDI, imm64 */
    xb(0xb8 | XR_DI);
    codegen_addquad(NULL, (uint64_t) (uintptr_t) &fpu_state);
    xb(0x0f); /* MOVZX ECX, word [RDI + cwd] */
    xb(0xb7);
    x_mem(XR_CX, XR_DI, X87H_FS_CWD);
    x_rr(0x89, XR_CX, XR_AX); /* MOV EAX, ECX */
    x_rr(0xf7, 2, XR_AX);     /* NOT EAX */
    xb(0xa8);                 /* TEST AL, 3Fh */
    xb(0x3f);
    x_fail(f, XCC_NZ);
    xb(0x81); /* AND ECX, 300h */
    xb(0xe1);
    xl(0x300);
    xb(0x81); /* CMP ECX, 100h */
    xb(0xf9);
    xl(0x100);
    x_fail(f, XCC_Z);
    xb(0xf6); /* TEST byte [RDI + swd], 80h */
    x_mem(0, XR_DI, X87H_FS_SWD);
    xb(0x80);
    x_fail(f, XCC_NZ);
    xb(0x0f); /* MOVZX ESI, byte [RDI + tos] */
    xb(0xb6);
    x_mem(XR_SI, XR_DI, X87H_FS_TOS);
}

/* The host x87 made ready for an operation: the guest's control word as the
   host's (0x7f | its precision and rounding), loaded if the host's own,
   saved for x_take_sw() to put back, is another; the host's flags cleared if
   the guest has cleared some of them since, or if the host holds one the
   guest was never given. */
static void
x_host_ready(void)
{
    uint8_t *j;

    xb(0x0f); /* MOVZX ECX, word [RDI + cwd] */
    xb(0xb7);
    x_mem(XR_CX, XR_DI, X87H_FS_CWD);
    xb(0x81); /* AND ECX, F00h */
    xb(0xe1);
    xl(0xf00);
    xb(0x83); /* OR ECX, 7Fh */
    xb(0xc9);
    xb(0x7f);
    xb(0x66); /* MOV [RBP + x87h_cw], CX */
    xb(0x89);
    x_mem(XR_CX, XR_BP, XCS(x87h_cw));
    xb(0xd9); /* FNSTCW [RBP + x87h_hcw] */
    x_mem(7, XR_BP, XCS(x87h_hcw));
    xb(0x66); /* CMP CX, [RBP + x87h_hcw] */
    xb(0x3b);
    x_mem(XR_CX, XR_BP, XCS(x87h_hcw));
    j = x_jcc8(XCC_Z);
    xb(0xd9); /* FLDCW [RBP + x87h_cw] */
    x_mem(5, XR_BP, XCS(x87h_cw));
    x_here8(j);

    /* Flags the host holds that the guest has cleared since. */
    xb(0x0f); /* MOVZX EAX, word [RBP + x87h_fl] */
    xb(0xb7);
    x_mem(XR_AX, XR_BP, XCS(x87h_fl));
    xb(0x0f); /* MOVZX ECX, word [RDI + swd] */
    xb(0xb7);
    x_mem(XR_CX, XR_DI, X87H_FS_SWD);
    x_rr(0xf7, 2, XR_CX);     /* NOT ECX */
    x_rr(0x21, XR_CX, XR_AX); /* AND EAX, ECX */
    xb(0xa8);                 /* TEST AL, 3Fh */
    xb(0x3f);
    j = x_jcc8(XCC_Z);
    xb(0xdb); /* FNCLEX */
    xb(0xe2);
    xb(0x66); /* MOV word [RBP + x87h_fl], 0 */
    xb(0xc7);
    x_mem(0, XR_BP, XCS(x87h_fl));
    xb(0);
    xb(0);
    x_here8(j);

    /* Flags the host holds that the guest was never given. */
    xb(0xdf); /* FNSTSW AX */
    xb(0xe0);
    xb(0x0f); /* MOVZX ECX, word [RBP + x87h_fl] */
    xb(0xb7);
    x_mem(XR_CX, XR_BP, XCS(x87h_fl));
    x_rr(0xf7, 2, XR_CX);     /* NOT ECX */
    x_rr(0x21, XR_CX, XR_AX); /* AND EAX, ECX */
    xb(0xa8);                 /* TEST AL, 3Fh */
    xb(0x3f);
    j = x_jcc8(XCC_Z);
    xb(0xdb); /* FNCLEX */
    xb(0xe2);
    x_here8(j);
}

/* FNSTSW [RBP + x87h_sw], straight after the operation (a store after it
   changes C1); then the host's own control word back, if x_host_ready()
   loaded the guest's. ECX only: RAX and RDX may hold slot addresses. */
static void
x_take_sw(void)
{
    uint8_t *j;

    xb(0xdd);
    x_mem(7, XR_BP, XCS(x87h_sw));
    xb(0x0f); /* MOVZX ECX, word [RBP + x87h_cw] */
    xb(0xb7);
    x_mem(XR_CX, XR_BP, XCS(x87h_cw));
    xb(0x66); /* CMP CX, [RBP + x87h_hcw] */
    xb(0x3b);
    x_mem(XR_CX, XR_BP, XCS(x87h_hcw));
    j = x_jcc8(XCC_Z);
    xb(0xd9); /* FLDCW [RBP + x87h_hcw] */
    x_mem(5, XR_BP, XCS(x87h_hcw));
    x_here8(j);
}

/* The status word merged: the host's flags (sticky, the guest's already
   among them) and C1, or, for a compare, C0, C2 and C3 with C1 clear; the
   flags the host holds noted. */
static void
x_merge(int compare)
{
    xb(0x0f); /* MOVZX EAX, word [RBP + x87h_sw] */
    xb(0xb7);
    x_mem(XR_AX, XR_BP, XCS(x87h_sw));
    x_rr(0x89, XR_AX, XR_CX); /* MOV ECX, EAX */
    xb(0x83);                 /* AND ECX, 3Fh */
    xb(0xe1);
    xb(0x3f);
    xb(0x66); /* MOV [RBP + x87h_fl], CX */
    xb(0x89);
    x_mem(XR_CX, XR_BP, XCS(x87h_fl));
    xb(0x25); /* AND EAX, mask */
    xl(compare ? 0x453f : 0x023f);
    xb(0x0f); /* MOVZX ECX, word [RDI + swd] */
    xb(0xb7);
    x_mem(XR_CX, XR_DI, X87H_FS_SWD);
    xb(0x81); /* AND ECX, ~mask */
    xb(0xe1);
    xl(compare ? ~0x4700u & 0xffff : ~0x0200u & 0xffff);
    x_rr(0x09, XR_AX, XR_CX); /* OR ECX, EAX */
    xb(0x66);                 /* MOV [RDI + swd], CX */
    xb(0x89);
    x_mem(XR_CX, XR_DI, X87H_FS_SWD);
}

/* An x87 instruction on the operand in cpu_state. */
static void
x_operand(uint8_t op, int digit, int size)
{
    xb(op);
    x_mem(digit, XR_BP, (size == 2) ? XCS(x87h_w) : (size == 4) ? XCS(x87h_l) : XCS(x87h_q));
}

/* An 80-bit load or store through a slot address in reg. */
static void
x_fld80(int reg)
{
    xb(0xdb);
    x_ind(5, reg);
}

static void
x_fstp80(int reg)
{
    xb(0xdb);
    x_ind(7, reg);
}

/* Copies ST(src)'s slot to the slot address in RDX (10 bytes). */
static void
x_copy_slot_to_rdx(int src)
{
    x_slot(XR_AX, src);
    xb(0x48); /* MOV RCX, [RAX] */
    xb(0x8b);
    x_ind(XR_CX, XR_AX);
    xb(0x48); /* MOV [RDX], RCX */
    xb(0x89);
    x_ind(XR_CX, XR_DX);
    xb(0x0f); /* MOVZX ECX, word [RAX + 8] */
    xb(0xb7);
    xb(0x40 | (XR_CX << 3) | XR_AX);
    xb(8);
    xb(0x66); /* MOV [RDX + 8], CX */
    xb(0x89);
    xb(0x40 | (XR_CX << 3) | XR_DX);
    xb(8);
}

static void
x87h_emit_mem(x87h_fails_t *f, int esc, int digit)
{
    const uint8_t op = 0xd8 + esc;
    int           size;
    const int     kind = x87h_mem_kind(esc, digit, &size);

    switch (kind) {
        case X87H_ARITH:
            x_tag_test(f, 0, 1);
            x_host_ready();
            x_slot(XR_DX, 0);
            x_fld80(XR_DX);
            x_operand(op, digit, size);
            x_take_sw();
            x_fstp80(XR_DX);
            x_tag_set(0, 0);
            x_merge(0);
            break;

        case X87H_COMPARE:
            x_tag_test(f, 0, 1);
            x_host_ready();
            x_slot(XR_DX, 0);
            x_fld80(XR_DX);
            x_operand(op, 3, size); /* FCOMP / FICOMP */
            x_take_sw();
            x_merge(1);
            if (x87h_mem_pops(esc, digit))
                x_pop();
            break;

        case X87H_LOAD:
            x_tag_test(f, 7, 0);
            x_host_ready();
            x_operand(op, digit, size); /* FLD / FILD */
            x_take_sw();
            x_slot(XR_DX, 7);
            x_fstp80(XR_DX);
            x_tos_move(-1);
            x_tag_set(0, 0);
            x_merge(0);
            break;

        case X87H_COPY_LOAD:
            x_tag_test(f, 7, 0);
            x_slot(XR_DX, 7);
            xb(0x48); /* MOV RCX, [RBP + x87h_q] */
            xb(0x8b);
            x_mem(XR_CX, XR_BP, XCS(x87h_q));
            xb(0x48); /* MOV [RDX], RCX */
            xb(0x89);
            x_ind(XR_CX, XR_DX);
            xb(0x0f); /* MOVZX ECX, word [RBP + x87h_w] */
            xb(0xb7);
            x_mem(XR_CX, XR_BP, XCS(x87h_w));
            xb(0x66); /* MOV [RDX + 8], CX */
            xb(0x89);
            xb(0x40 | (XR_CX << 3) | XR_DX);
            xb(8);
            x_tos_move(-1);
            x_tag_set(0, 0);
            x_clear_c1();
            break;

        case X87H_STORE:
            x_tag_test(f, 0, 1);
            x_host_ready();
            x_slot(XR_DX, 0);
            x_fld80(XR_DX);
            x_operand(op, ((esc == 7) && (digit == 7)) ? 7 : 3, size); /* FSTP / FISTP */
            x_take_sw();
            break;

        case X87H_COPY_STORE:
        default:
            x_tag_test(f, 0, 1);
            x_slot(XR_AX, 0);
            xb(0x48); /* MOV RCX, [RAX] */
            xb(0x8b);
            x_ind(XR_CX, XR_AX);
            xb(0x48); /* MOV [RBP + x87h_q], RCX */
            xb(0x89);
            x_mem(XR_CX, XR_BP, XCS(x87h_q));
            xb(0x0f); /* MOVZX ECX, word [RAX + 8] */
            xb(0xb7);
            xb(0x40 | (XR_CX << 3) | XR_AX);
            xb(8);
            xb(0x66); /* MOV [RBP + x87h_w], CX */
            xb(0x89);
            x_mem(XR_CX, XR_BP, XCS(x87h_w));
            break;
    }
}

static void
x87h_emit_body(x87h_fails_t *f, uint32_t d)
{
    const int esc   = X87H_DESC_ESC(d);
    const int digit = X87H_DESC_DIGIT(d);
    const int i     = X87H_DESC_I(d);
    const int x     = X87H_DESC_X(d);

    switch (X87H_DESC_OP(d)) {
        case X87H_OP_MEM:
            x87h_emit_mem(f, esc, digit);
            break;

        case X87H_OP_ARITH_REG:
            /* ST(i) as the host's ST(1), ST(0) as its ST(0), the guest's
               opcode with i = 1. D8: ST(0) = ST(0) op ST(1); DC: ST(1) =
               ST(1) op ST(0); DE: the same, the host's ST(0) popped. */
            x_tag_test(f, 0, 1);
            x_tag_test(f, i, 1);
            x_host_ready();
            x_slot(XR_DX, i);
            x_fld80(XR_DX);
            x_slot(XR_AX, 0);
            x_fld80(XR_AX);
            xb(0xd8 + esc);
            xb(0xc0 | (digit << 3) | 1);
            x_take_sw();
            if (esc == 0) {
                x_fstp80(XR_AX);
                xb(0xdd); /* FSTP ST(0) */
                xb(0xd8);
                x_tag_set(0, 0);
            } else {
                if (esc == 4) {
                    xb(0xdd);
                    xb(0xd8);
                }
                x_fstp80(XR_DX);
                x_tag_set(i, 0);
            }
            x_merge(0);
            if (esc == 6)
                x_pop();
            break;

        case X87H_OP_FCOM_REG:
            x_tag_test(f, 0, 1);
            x_tag_test(f, i, 1);
            x_host_ready();
            x_slot(XR_DX, i);
            x_fld80(XR_DX);
            x_slot(XR_AX, 0);
            x_fld80(XR_AX);
            xb((x & 4) ? 0xdd : 0xd8); /* FUCOM / FCOM ST(1) */
            xb((x & 4) ? 0xe1 : 0xd1);
            x_take_sw();
            xb(0xdd); /* FSTP ST(0), twice */
            xb(0xd8);
            xb(0xdd);
            xb(0xd8);
            x_merge(1);
            for (int n = 0; n < (x & 3); n++)
                x_pop();
            break;

        case X87H_OP_FXCH:
            x_tag_test(f, 0, 1);
            x_tag_test(f, i, 1);
            x_slot(XR_AX, 0);
            x_slot(XR_DX, i);
            /* Plain moves: XCHG with memory is a locked operation. RSI (the
               stack top) is not needed again. */
            xb(0x48); /* MOV RCX, [RAX] */
            xb(0x8b);
            x_ind(XR_CX, XR_AX);
            xb(0x48); /* MOV RSI, [RDX] */
            xb(0x8b);
            x_ind(XR_SI, XR_DX);
            xb(0x48); /* MOV [RAX], RSI */
            xb(0x89);
            x_ind(XR_SI, XR_AX);
            xb(0x48); /* MOV [RDX], RCX */
            xb(0x89);
            x_ind(XR_CX, XR_DX);
            xb(0x0f); /* MOVZX ECX, word [RAX + 8] */
            xb(0xb7);
            xb(0x40 | (XR_CX << 3) | XR_AX);
            xb(8);
            xb(0x0f); /* MOVZX ESI, word [RDX + 8] */
            xb(0xb7);
            xb(0x40 | (XR_SI << 3) | XR_DX);
            xb(8);
            xb(0x66); /* MOV [RAX + 8], SI */
            xb(0x89);
            xb(0x40 | (XR_SI << 3) | XR_AX);
            xb(8);
            xb(0x66); /* MOV [RDX + 8], CX */
            xb(0x89);
            xb(0x40 | (XR_CX << 3) | XR_DX);
            xb(8);
            x_clear_c1();
            break;

        case X87H_OP_FLD_ST:
            x_tag_test(f, i, 1);
            x_tag_test(f, 7, 0);
            x_slot(XR_DX, 7);
            x_copy_slot_to_rdx(i);
            x_tos_move(-1);
            x_tag_set(0, 0);
            x_clear_c1();
            break;

        case X87H_OP_FST_ST:
            x_tag_test(f, 0, 1);
            x_slot(XR_DX, i);
            x_copy_slot_to_rdx(0);
            x_tag_set(i, 0);
            x_clear_c1();
            if (x & 1)
                x_pop();
            break;

        case X87H_OP_SIGN:
            x_tag_test(f, 0, 1);
            x_slot(XR_AX, 0);
            if (x & 1) { /* FABS: AND byte [RAX + 9], 7Fh */
                xb(0x80);
                xb(0x40 | (4 << 3) | XR_AX);
                xb(9);
                xb(0x7f);
            } else { /* FCHS: XOR byte [RAX + 9], 80h */
                xb(0x80);
                xb(0x40 | (6 << 3) | XR_AX);
                xb(9);
                xb(0x80);
            }
            x_clear_c1();
            break;

        case X87H_OP_CONST:
            x_tag_test(f, 7, 0);
            x_slot(XR_DX, 7);
            xb(0x48); /* MOV RCX, imm64 */
            xb(0xb8 | XR_CX);
            codegen_addquad(NULL, (x & 1) ? 0x8000000000000000ull : 0);
            xb(0x48); /* MOV [RDX], RCX */
            xb(0x89);
            x_ind(XR_CX, XR_DX);
            xb(0x66); /* MOV word [RDX + 8], imm16 */
            xb(0xc7);
            xb(0x40 | XR_DX);
            xb(8);
            xb((x & 1) ? 0xff : 0x00);
            xb((x & 1) ? 0x3f : 0x00);
            x_tos_move(-1);
            x_tag_set(0, 0);
            x_clear_c1();
            break;

        case X87H_OP_FSQRT:
        default:
            x_tag_test(f, 0, 1);
            x_host_ready();
            x_slot(XR_DX, 0);
            x_fld80(XR_DX);
            xb(0xd9); /* FSQRT */
            xb(0xfa);
            x_take_sw();
            x_fstp80(XR_DX);
            x_tag_set(0, 0);
            x_merge(0);
            break;
    }
}

/* The most code one instruction takes, with room to spare: it is allocated
   at once, so no jump in it crosses to another piece of the block. */
#    define X87H_MAX_BYTES 640

static int
codegen_X87H(codeblock_t *block, uop_t *uop)
{
    x87h_fails_t f = { { NULL }, 0 };
    uint8_t     *done;

    codegen_alloc_bytes(block, X87H_MAX_BYTES);
    x_enter(&f);
    x87h_emit_body(&f, uop->imm_data);
    xb(0xeb); /* JMP short, past the jump to the handler's call */
    xb(0);
    done = &block_write_data[block_pos - 1];
    for (int n = 0; n < f.n; n++) {
        uint8_t *p = f.fail[n];

        *(uint32_t *) p = (uint32_t) (&block_write_data[block_pos] - (p + 4));
    }
    xb(0xe9); /* JMP rel32: to the handler's call (the IR's jump) */
    xl(0);
    uop->p = &block_write_data[block_pos - 4];
    x_here8(done);

    return 0;
}

/* A store's value is in memory: its flags, and the pop of the popping
   forms. */
static int
codegen_X87H_DONE(codeblock_t *block, uop_t *uop)
{
    const int esc   = X87H_DESC_ESC(uop->imm_data);
    const int digit = X87H_DESC_DIGIT(uop->imm_data);
    int       size;

    codegen_alloc_bytes(block, X87H_MAX_BYTES);
    xb(0x48); /* MOV RDI, imm64 */
    xb(0xb8 | XR_DI);
    codegen_addquad(NULL, (uint64_t) (uintptr_t) &fpu_state);
    xb(0x0f); /* MOVZX ESI, byte [RDI + tos] */
    xb(0xb6);
    x_mem(XR_SI, XR_DI, X87H_FS_TOS);
    if (x87h_mem_kind(esc, digit, &size) == X87H_COPY_STORE) {
        x_clear_c1();
        x_pop();
    } else {
        x_merge(0);
        if (x87h_mem_pops(esc, digit))
            x_pop();
    }

    return 0;
}

#endif /* X87_HOST_AVAILABLE */
