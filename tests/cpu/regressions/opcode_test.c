/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Executes the production decoder, MMU and both execution engines. */
#define main previous_regression_main
#define outl previous_outl
#define timer_process previous_timer_process
#include "simd_exception_test.c"
#undef main
#undef outl
#undef timer_process
#include "x87_sf.h"
#include "x87.h"

typedef struct {
    const char *name;
    uint8_t bytes[32];
    unsigned length, cr0_bits, no_osfxsr, trap_p3, trap_p4, setup, only_p4;
} opcode_case;
static const opcode_case opcases[] = {
    {"LFENCE r/m0", {0x0f,0xae,0xe8},3,0,0,6,0},
    {"LFENCE r/m3", {0x0f,0xae,0xeb},3,0,0,6,0},
    {"LFENCE r/m7", {0x0f,0xae,0xef},3,0,0,6,0},
    {"LFENCE memory invalid", {0x0f,0xae,0x28},3,0,0,6,6},
    {"MFENCE r/m0", {0x0f,0xae,0xf0},3,0,0,6,0},
    {"MFENCE r/m3", {0x0f,0xae,0xf3},3,0,0,6,0},
    {"MFENCE r/m7", {0x0f,0xae,0xf7},3,0,0,6,0},
    {"MFENCE memory invalid", {0x0f,0xae,0x30},3,0,0,6,6},
    {"SFENCE r/m0", {0x0f,0xae,0xf8},3,0,0,0,0},
    {"SFENCE r/m3", {0x0f,0xae,0xfb},3,0,0,0,0},
    {"SFENCE r/m7", {0x0f,0xae,0xff},3,0,0,0,0},
    {"CLFLUSH feature", {0x0f,0xae,0x38},3,0,0,6,0},
    {"FXSAVE register a32 TS0", {0x8b,0x15,0x1,0x2,0x0,0x0,0xf,0xae,0xc0},9,0,0,6,6,16},
    {"FXSAVE register a32 TS1", {0x8b,0x15,0x1,0x2,0x0,0x0,0xf,0xae,0xc0},9,8,0,6,6,16},
    {"FXSAVE register a16 TS0", {0x8b,0x15,0x1,0x2,0x0,0x0,0x67,0xf,0xae,0xc0},10,0,0,6,6,16},
    {"FXSAVE register a16 TS1", {0x8b,0x15,0x1,0x2,0x0,0x0,0x67,0xf,0xae,0xc0},10,8,0,6,6,16},
    {"FXRSTOR register a32 TS0", {0x8b,0x15,0x1,0x2,0x0,0x0,0xf,0xae,0xc8},9,0,0,6,6,16},
    {"FXRSTOR register a32 TS1", {0x8b,0x15,0x1,0x2,0x0,0x0,0xf,0xae,0xc8},9,8,0,6,6,16},
    {"FXRSTOR register a16 TS0", {0x8b,0x15,0x1,0x2,0x0,0x0,0x67,0xf,0xae,0xc8},10,0,0,6,6,16},
    {"FXRSTOR register a16 TS1", {0x8b,0x15,0x1,0x2,0x0,0x0,0x67,0xf,0xae,0xc8},10,8,0,6,6,16},
    {"FXRSTOR FXSAVE environment", {0x0f,0xae,0x08,0x0f,0xae,0x00},6,0,0,0,0,15},
    {"FXRSTOR pointer width a32 o32", {0xf,0xae,0x8},3,0,0,0,0,4},
    {"FXSAVE pointer width a32 o32", {0xf,0xae,0x0},3,0,0,0,0,14},
    {"FXRSTOR pointer width a32 o16", {0x66,0xf,0xae,0x8},4,0,0,0,0,4},
    {"FXSAVE pointer width a32 o16", {0x66,0xf,0xae,0x0},4,0,0,0,0,14},
    {"FXRSTOR pointer width a16 o32", {0x67,0xf,0xae,0xf},4,0,0,0,0,4},
    {"FXSAVE pointer width a16 o32", {0x67,0xf,0xae,0x7},4,0,0,0,0,14},
    {"FXRSTOR pointer width a16 o16", {0x67,0x66,0xf,0xae,0xf},5,0,0,0,0,4},
    {"FXSAVE pointer width a16 o16", {0x67,0x66,0xf,0xae,0x7},5,0,0,0,0,14},
    {"FXSAVE edit FTW FXRSTOR", {0x0f,0x6f,0xc1,0x0f,0xae,0x00,0xc6,0x40,0x04,0x00,0x0f,0xae,0x08},13,0,0,0,0},
    {"FXRSTOR reserved tail", {0x0f,0xae,0x08},3,0,0,0,0,5},
    {"FXRSTOR mixed physical tags", {0x0f,0xae,0x08,0x0f,0xae,0x00},6,0,0,0,0,13},
    {"FXRSTOR MMX payload", {0x0f,0x6f,0xc1,0x0f,0xae,0x00,0x0f,0x77,0x0f,0xae,0x08},11,0,0,0,0},
    {"MMX pending x87 exception", {0x0f,0x6f,0xc1},3,0x20,0,16,16,9},
    {"MMX pending x87 exception in counted loop", {0x0f,0xfe,0xc1,0x49,0x75,0xfa},6,0x20,0,16,16,27},
    {"EMMS pending x87 exception", {0x0f,0x77},2,0x20,0,16,16,9},
    {"FWAIT pending control", {0x9b},1,0x20,0,16,16,9},
    {"ADDPS pending x87 control", {0x0f,0x58,0xc1},3,0x20,0,0,0,9},
    {"MMX fault MOVQ", {0xf,0x6f,0x7},3,0x80010000,0,14,14,12},
    {"MMX fault MOVD", {0xf,0x6e,0x7},3,0x80010000,0,14,14,12},
    {"MMX fault PADDD", {0xf,0xfe,0x7},3,0x80010000,0,14,14,12},
    {"MMX fault PCMPEQD", {0xf,0x76,0x7},3,0x80010000,0,14,14,12},
    {"MMX fault PAND", {0xf,0xdb,0x7},3,0x80010000,0,14,14,12},
    {"MMX fault PACKSSWB", {0xf,0x63,0x7},3,0x80010000,0,14,14,12},
    {"MMX fault PSLLQ", {0xf,0xf3,0x7},3,0x80010000,0,14,14,12},
    {"MMX fault PSHUFW", {0xf,0x70,0x7,0x0},4,0x80010000,0,14,14,12},
    {"MMX fault PINSRW", {0xf,0xc4,0x7,0x0},4,0x80010000,0,14,14,12},
    {"MMX fault MOVQ store", {0xf,0x7f,0x7},3,0x80010000,0,14,14,12},
    {"MMX fault MOVD store", {0xf,0x7e,0x7},3,0x80010000,0,14,14,12},
    {"MOVQ2DQ normal a32", {0xf3,0xf,0xd6,0xc1},4,0,0,6,0,0,1},
    {"MOVQ2DQ TS a32", {0xf3,0xf,0xd6,0xc1},4,8,0,6,7,0,1},
    {"MOVQ2DQ EM a32", {0xf3,0xf,0xd6,0xc1},4,4,0,6,6,0,1},
    {"MOVQ2DQ OSFXSR off a32", {0xf3,0xf,0xd6,0xc1},4,0,1,6,6,0,1},
    {"MOVQ2DQ normal a16", {0x67,0xf3,0xf,0xd6,0xc1},5,0,0,6,0,0,1},
    {"MOVQ2DQ TS a16", {0x67,0xf3,0xf,0xd6,0xc1},5,8,0,6,7,0,1},
    {"MOVQ2DQ EM a16", {0x67,0xf3,0xf,0xd6,0xc1},5,4,0,6,6,0,1},
    {"MOVQ2DQ OSFXSR off a16", {0x67,0xf3,0xf,0xd6,0xc1},5,0,1,6,6,0,1},
    {"MOVDQ2Q normal a32", {0xf2,0xf,0xd6,0xc1},4,0,0,6,0,0,1},
    {"MOVDQ2Q TS a32", {0xf2,0xf,0xd6,0xc1},4,8,0,6,7,0,1},
    {"MOVDQ2Q EM a32", {0xf2,0xf,0xd6,0xc1},4,4,0,6,6,0,1},
    {"MOVDQ2Q OSFXSR off a32", {0xf2,0xf,0xd6,0xc1},4,0,1,6,6,0,1},
    {"MOVDQ2Q normal a16", {0x67,0xf2,0xf,0xd6,0xc1},5,0,0,6,0,0,1},
    {"MOVDQ2Q TS a16", {0x67,0xf2,0xf,0xd6,0xc1},5,8,0,6,7,0,1},
    {"MOVDQ2Q EM a16", {0x67,0xf2,0xf,0xd6,0xc1},5,4,0,6,6,0,1},
    {"MOVDQ2Q OSFXSR off a16", {0x67,0xf2,0xf,0xd6,0xc1},5,0,1,6,6,0,1},
    {"CVTPI2PD normal a32", {0x66,0xf,0x2a,0xc1},4,0,0,6,0,0,1},
    {"CVTPI2PD TS a32", {0x66,0xf,0x2a,0xc1},4,8,0,6,7,0,1},
    {"CVTPI2PD EM a32", {0x66,0xf,0x2a,0xc1},4,4,0,6,6,0,1},
    {"CVTPI2PD OSFXSR off a32", {0x66,0xf,0x2a,0xc1},4,0,1,6,6,0,1},
    {"CVTPI2PD normal a16", {0x67,0x66,0xf,0x2a,0xc1},5,0,0,6,0,0,1},
    {"CVTPI2PD TS a16", {0x67,0x66,0xf,0x2a,0xc1},5,8,0,6,7,0,1},
    {"CVTPI2PD EM a16", {0x67,0x66,0xf,0x2a,0xc1},5,4,0,6,6,0,1},
    {"CVTPI2PD OSFXSR off a16", {0x67,0x66,0xf,0x2a,0xc1},5,0,1,6,6,0,1},
    {"LDMXCSR normal", {0x0f,0xae,0x10},3,0,0,0,0},
    {"LDMXCSR TS", {0x0f,0xae,0x10},3,8,0,7,7},
    {"LDMXCSR EM", {0x0f,0xae,0x10},3,4,0,6,6},
    {"LDMXCSR OSFXSR off", {0x0f,0xae,0x10},3,0,1,6,6},
    {"STMXCSR TS", {0x0f,0xae,0x18},3,8,0,7,7},
    {"STMXCSR EM", {0x0f,0xae,0x18},3,4,0,6,6},
    {"STMXCSR OSFXSR off", {0x0f,0xae,0x18},3,0,1,6,6},
    {"MOVQ xmm absent a32", {0xf3,0x0f,0x7e,0x07},4,0x80010000,0,6,14,3,1},
    {"MOVQ xmm absent a16", {0x64,0x67,0xf3,0x0f,0x7e,0x07},6,0x80010000,0,6,14,3,1},
    {"MOVQ xmm cross-page", {0xf3,0x0f,0x7e,0x07},4,0x80010000,0,6,14,11,1},
    {"MOVQ xmm valid", {0xf3,0x0f,0x7e,0x00},4,0,0,6,0,0,1},
    {"MASKMOVQ FS limit a32", {0x8b,0x15,0,2,0,0,0x64,0x0f,0xf7,0xc1},10,0,0,13,13,1},
    {"MASKMOVQ FS limit a16", {0x8b,0x15,0,2,0,0,0x64,0x67,0x0f,0xf7,0xc1},11,0,0,13,13,1},
    {"MASKMOVDQU FS limit a32", {0x8b,0x15,0,2,0,0,0x64,0x66,0x0f,0xf7,0xc1},11,0,0,6,13,1,1},
    {"MASKMOVDQU FS limit a16", {0x8b,0x15,0,2,0,0,0x64,0x67,0x66,0x0f,0xf7,0xc1},12,0,0,6,13,1,1},
    {"MASKMOVQ valid", {0x0f,0xf7,0xc1},3,0,0,0,0},
    {"MASKMOVDQU valid", {0x66,0x0f,0xf7,0xc1},4,0,0,6,0,0,1},
};
static const opcode_case *probe;
static unsigned finished_compiled;
void outl(uint16_t port, uint32_t value) {
    (void)value;
    if (port != 0xec) fatal("Unexpected audit port %x", port);
    finished_compiled += !!inrecomp;
    done = 1;
    cycles = cycles_main = 0;
    cpu_block_end = 1;
}
void timer_process(void) {
    timer_target = tsc + 1000;
    if (tsc > 100000000 || x86_was_reset || cpu_state.abrt)
        fatal("Audit watchdog: %s", probe ? probe->name : "setup");
}
static void initialize(void) {
    resetx86(); codegen_reset();
    cr0 = 1; cr4 = CR4_OSFXSR | CR4_OSXMMEXCPT;
    flat_segment(&cpu_state.seg_cs,8,0x9b);
    flat_segment(&cpu_state.seg_ds,16,0x93); flat_segment(&cpu_state.seg_es,16,0x93);
    flat_segment(&cpu_state.seg_ss,16,0x93); flat_segment(&cpu_state.seg_fs,16,0x93);
    flat_segment(&cpu_state.seg_gs,16,0x93);
    cpu_cur_status = CPU_STATUS_PMODE | CPU_STATUS_USE32 | CPU_STATUS_STACK32;
    use32=0x300; stack32=1; cpu_state.flags=2; cpu_state.eflags=0;
    cycles=cycles_main=0; tsc=0; timer_target=1000; x86_was_reset=0;
    mem_a20_alt=1; mem_a20_recalc(); flushmmucache();
    memset(ram+DATA,0,0x2000);
    emit_pos=DRIVER; b(0xe8); d(BODY-(DRIVER+5)); b(0xe7); b(0xec); b(0xeb); b(0xfe);
    emit_pos=BODY;
    for (unsigned i=0;i<probe->length;i++) b(probe->bytes[i]);
    if(probe->setup==6 || probe->setup==7 || probe->setup==8) {
        b(0x0f);b(0x31);b(0xa3);d(RESULT+32);
        for(int i=0;i<(probe->setup==6?64:probe->setup==8?32:0);i++){b(0x83);b(0xc3);b(1);}
        b(0x0f);b(0x31);b(0xa3);d(RESULT+36);
        b(0x0f);b(0x31);b(0xa3);d(RESULT+40);
    }
    resume_pc=emit_pos;
    b(0xc3);
    exception_tables();
    if (probe->setup==1) cpu_state.seg_fs.limit=cpu_state.seg_fs.limit_high=0xfff;
    if (probe->setup==2 || probe->setup==3 || probe->setup==11 || probe->setup==12) {
        memset(ram+0x30000,0,0x5000);
        for (unsigned n=0;n<4;n++) {
            put32(0x30000+4*n,(0x31000+0x1000*n)|3);
            for (unsigned p=0;p<1024;p++) put32(0x31000+0x1000*n+4*p,((n*1024+p)*4096)|3);
        }
        put32(0x31000+4*(probe->setup==11 ? 0x41 : 0x40),probe->setup==2 ? 0x40001 : 0);
        cr3=0x30000;
    }
}
#ifndef AUDIT_OPCODE_NO_MAIN
int main(int argc,char **argv) {
    int p3=argc>1&&!strcmp(argv[1],"p3");
    engine=argc>2?atoi(argv[2]):0; fpu_softfloat=argc>3?atoi(argv[3]):0;
    cpu_dyn_accurate_fpu_env=argc>5?atoi(argv[5]):0;
    cpu_f=cpu_get_family(p3?"pentium3_katmai":"pentium4w"); cpu=p3?0:4;
    fpu_type=FPU_INTERNAL; cpu_use_dynarec=engine;
    cpu_set(); codegen_init(); mem_size=16*1024; mem_init(); mem_reset();
    unsigned checked = 0;
    for(unsigned i=0;i<sizeof(opcases)/sizeof(opcases[0]);i++) {
        probe=&opcases[i]; if(argc>4&&!strstr(probe->name,argv[4]))continue;
        if (p3 && probe->only_p4) continue;
        initialize(); finished_compiled=0;
        for(unsigned call=0;call<64;call++) {
            cr0=1|probe->cr0_bits; cr4=CR4_OSXMMEXCPT|(probe->no_osfxsr?0:CR4_OSFXSR);
            cpu_state.mxcsr=0x1f80; cpu_state.flags=2; cpu_state.eflags=0;
            cpu_state.npxc=fpu_state.cwd=0x37f; cpu_state.npxs=fpu_state.swd=0;
            cpu_state.ismmx=0;
            cpu_state.TOP=3; x87_settag(0xffff); fpu_state.tos=3; fpu_state.tag=0xffff;
            cpu_state.ssegs=0;
            cpu_state.eaaddr=DATA; easeg=0; EAX=DATA; EDI=DATA+0x800; ESP=0x700000;
            ECX=0x12345678; EDX=0;
            memset(&cpu_state.XMM[0],0x11,16); memset(&cpu_state.XMM[1],0x80,16);
            MMP[0]->q=UINT64_C(0x1111111111111111); MMP[1]->q=UINT64_C(0x8080808080808080);
            cpu_state.MM[0].q=UINT64_C(0x1111111111111111); cpu_state.MM[1].q=UINT64_C(0x8080808080808080);
            unsigned mask = (call / 8) % 3;
            if (strstr(probe->name, "MASKMOV")) {
                for (unsigned n = 0; n < 16; n++) cpu_state.XMM[1].b[n] = mask == 0 ? 0 : mask == 1 ? 0x80 : (n & 1) * 0x80;
                for (unsigned n = 0; n < 8; n++) MMP[1]->b[n] = cpu_state.XMM[1].b[n];
                memset(MMP[0], 0x11, 8);
                memset(ram + DATA + 0x800, 0x5a, 16);
                memset(ram + DATA + 0x1800, 0x5a, 16);
            }
            put32(DATA,0x3f80); put32(TRAP,0); put32(FAULT_PC,0);
            if (probe->setup==2 || probe->setup==3 || probe->setup==11 || probe->setup==12) {
                cpu_state.seg_fs.base=0x40000; EBX=0;
                EDI=probe->setup==11 ? 0x40ffc : 0x40000; EAX=0x1234; put32(0x40000,0x89abcdef); put32(0x40004,0x76543210);
                flushmmucache();
            }
            if (probe->setup==4 || probe->setup==5 || probe->setup==13 || probe->setup==14 || probe->setup==15) {
                EAX=EBX=0x4000;
                memset(ram+0x4000,0,512);
                put32(0x4000,0x037f); put32(0x4008,0x12345678); put32(0x4010,0x87654321); put32(0x4018,0x1f80);
                if (probe->setup==15) {
                    ram[0x4006]=0x23;ram[0x4007]=1;ram[0x400c]=0x28;ram[0x4014]=0x30;
                    cpu_state.fpu_pc=0;cpu_state.fpu_ea=0;cpu_state.fpu_op=0;cpu_state.fpu_CS=cpu_state.fpu_DS=0;
                }
                if (probe->setup==14) {
                    cpu_state.fpu_pc=0x12345678;cpu_state.fpu_ea=0x87654321;
                }
                if (probe->setup==13) {
                    ram[0x4003]=0x18; ram[0x4004]=0xa5;
                    for(unsigned n=0;n<8;n++){put32(0x4020+16*n+4,0x80000000);ram[0x4020+16*n+8]=0xff;ram[0x4020+16*n+9]=0x3f;}
                    memset(ram+0x4000+400,0x5a,112);
                }
                if(probe->setup==5){put32(0x4000+400,0x54474553);memset(ram+0x4000+404,1,9);}
            }
            if (probe->setup==9 || probe->setup==27) {
                cpu_state.npxc=fpu_state.cwd=0x37e;cpu_state.npxs=fpu_state.swd=((call / 8) & 1) ? 0x8081 : 0;
            }
            if (probe->setup==27) {
                ECX=32; cpu_state.flags=2|(call&1); cpu_state.flags_op=0;
                MMP[0]->q=0; MMP[1]->q=UINT64_C(0x0000000200000001);
            }
            if (probe->setup == 12) {
                put32(0x31000+4*0x40, ((call / 8) & 1) ? 0 : 0x40003);
                flushmmucache();
            }
            if (probe->setup == 1) EDI = DATA + 0x1800;
            cpu_state.pc=DRIVER; done=0; cycles=cycles_main=0;
            for(unsigned n=0;!done;n++) {
                if(n>10000)fatal("Audit did not stop");
                if(engine) exec386_dynarec(10000); else exec386(10000);
            }
            unsigned expected = p3 ? probe->trap_p3 : probe->trap_p4;
            if ((probe->setup == 12 || probe->setup == 9 || probe->setup == 27) && !((call / 8) & 1)) expected = 0;
            if (get32(TRAP) != expected) {
                fprintf(stderr, "%s call %u: trap %u, expected %u\n", probe->name, call, get32(TRAP), expected);
                return 1;
            }
            if (strstr(probe->name, "MASKMOV")) {
                if (expected && get32(FAULT_PC) != BODY + 6) fatal("Masked store fault EIP");
                unsigned width = strstr(probe->name, "DQU") ? 16 : 8;
                unsigned address = DATA + (expected ? 0x1800 : 0x800);
                for (unsigned n = 0; n < width; n++) {
                    unsigned written = !expected && (mask == 1 || (mask == 2 && (n & 1)));
                    if (ram[address + n] != (written ? 0x11 : 0x5a)) fatal("Masked store byte %u", n);
                }
            }
            if (strstr(probe->name, "MOVQ xmm")) {
                if (expected) {
                    for (unsigned n=0;n<4;n++) if(cpu_state.XMM[0].l[n]!=0x11111111) fatal("MOVQ changed destination on fault");
                    if (get32(FAULT_PC)!=BODY) fatal("MOVQ fault EIP");
                } else if (cpu_state.XMM[0].q[0]!=0x3f80 || cpu_state.XMM[0].q[1]) fatal("MOVQ result");
            }
            if (probe->setup == 12) {
                unsigned top = fpu_softfloat ? fpu_state.tos : cpu_state.TOP;
                unsigned tag = fpu_softfloat ? fpu_state.tag : x87_gettag();
                if (expected && (top != 3 || tag != 0xffff || cpu_state.ismmx)) fatal("Faulting MMX changed x87 state: %s", probe->name);
                if (!expected && (top || !cpu_state.ismmx)) fatal("Successful MMX did not enter MMX mode");
                if (expected && get32(FAULT_PC)!=BODY) fatal("MMX fault EIP");
            }
            if ((probe->setup == 9 || probe->setup == 27) && expected) {
                if (get32(FAULT_PC)!=BODY || cpu_state.ismmx || (fpu_softfloat ? fpu_state.tos : cpu_state.TOP)!=3)
                    fatal("Pending x87 exception changed MMX state or EIP");
            }
            if (probe->setup == 27) {
                if (MMP[0]->q != (expected ? 0 : UINT64_C(0x0000004000000020)))
                    fatal("Counted MMX loop changed its result or committed a faulting instruction");
                if (!expected && (ECX || (cpu_state.flags & 1) != (call & 1)))
                    fatal("Counted MMX loop changed carry or failed to finish");
                if (expected && (fpu_softfloat ? fpu_state.tag : x87_gettag()) != 0xffff)
                    fatal("Counted MMX loop changed tags before #MF");
            }
            if (strstr(probe->name, "edit FTW") || probe->setup==5) {
                if ((fpu_softfloat ? fpu_state.tag : x87_gettag()) != 0xffff) fatal("FXRSTOR ignored empty FTW");
            }
            if (probe->setup==13) {
                if(ram[0x4004]!=0xa5 || (get32(0x4000)&0x38000000)!=0x18000000) fatal("FXRSTOR physical tags/TOP");
                for(unsigned n=400;n<512;n++) if(ram[0x4000+n]!=0x5a) fatal("FXSAVE wrote private state");
            }
            if (strstr(probe->name, "MMX payload") && MMP[0]->q!=UINT64_C(0x8080808080808080)) fatal("FXRSTOR lost MMX payload");
            if (probe->setup==4) {
                if ((fpu_softfloat ? fpu_state.fip : x87_pc_off)!=0x12345678 || (fpu_softfloat ? fpu_state.fdp : x87_op_off)!=0x87654321) fatal("FXRSTOR truncated pointers");
            }
            if (probe->setup==14 && (get32(0x4008)!=0x12345678 || get32(0x4010)!=0x87654321)) fatal("FXSAVE truncated pointers");
            if (probe->setup==15) {
                if(get32(0x4008)!=0x12345678 || get32(0x4010)!=0x87654321 || ram[0x4006]!=0x23 || ram[0x4007]!=1 || ram[0x400c]!=0x28 || ram[0x4014]!=0x30) fatal("FXRSTOR/FXSAVE lost environment");
            }
            if (probe->setup==16 && get32(FAULT_PC)!=BODY+6) fatal("FX register form fault EIP");
            ++checked;
        }

    }
    printf("%u opcode executions passed\n", checked);
    return 0;
}
#endif
