/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdarg.h>
#include <stdlib.h>
#include "../../../src/cpu/codegen_timing_p6.c"
cpu_state_t cpu_state;
static CPU model={.cpu_type=CPU_PENTIUM3};
CPU *cpu_s=&model;
int codegen_block_cycles;
void fatal(const char *fmt, ...) {
    va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap); exit(1);
}
#define CHECK(c) do { if (!(c)) fatal("line %d: %s\n",__LINE__,#c); } while (0)
static codegen_timing_t *timing=&codegen_timing_p6;
static void begin(void) { timing->start(); timing->block_start(); codegen_block_cycles=0; }
static void emit(uint8_t prefix, uint8_t opcode, uint32_t modrm) {
    timing->start();
    if (prefix) timing->prefix(prefix,modrm);
    timing->prefix(0x0f,modrm); timing->opcode(opcode,modrm,0x300,0);
}
static int measure(uint8_t prefix,uint8_t op,int n,int chain) {
    begin();
    for (int i=0;i<n;++i) emit(prefix,op,chain ? 0xc0 : 0xc7|((i%7)<<3));
    timing->block_end(); return codegen_block_cycles;
}
typedef struct { const char *name; uint8_t prefix,opcode; int latency,throughput; } anchor;
static void verify(const anchor *cases,unsigned count) {
    for (unsigned i=0;i<count;++i) {
        const anchor *a=&cases[i];
        int latency=measure(a->prefix,a->opcode,420,1)-measure(a->prefix,a->opcode,210,1);
        int throughput=measure(a->prefix,a->opcode,420,0)-measure(a->prefix,a->opcode,210,0);
        if (latency != 210*a->latency || throughput != 210*a->throughput)
            fatal("%s: latency %.3f/%d throughput %.3f/%d\n",a->name,latency/210.0,a->latency,throughput/210.0,a->throughput);
    }
}
int main(void) {
    /* Independent expected values from Fog's Pentium III tables, pp.187-188.
     * Long streams separate dependency latency from reciprocal throughput. */
    static const anchor p3[]={
        {"ADDPS",0,0x58,3,2},{"ADDSS",0xf3,0x58,3,1},
        {"MULPS",0,0x59,4,2},{"MULSS",0xf3,0x59,4,1},
        {"DIVPS",0,0x5e,48,34},{"DIVSS",0xf3,0x5e,18,17},
        {"SQRTPS",0,0x51,56,56},{"SQRTSS",0xf3,0x51,30,28},
        {"ANDPS",0,0x54,2,2},{"CMPPS",0,0xc2,3,2},
        {"MOVAPS",0,0x28,1,1},{"RCPPS",0,0x53,2,2}
    };
    verify(p3,sizeof(p3)/sizeof(p3[0]));
    begin(); reg_available_timestamp[0]=20;
    emit(0,0x58,0x00); CHECK(xmm_timestamp[0]>=25); /* ADDPS xmm0,[eax] */
    begin(); xmm_timestamp[0]=30;
    emit(0,0x28,0xc1); CHECK(xmm_timestamp[0]==1); /* MOVAPS overwrites. */
    begin(); xmm_timestamp[1]=20;
    emit(0,0x50,0xc1); CHECK(reg_available_timestamp[0]==21); /* MOVMSKPS bridge. */
    begin(); emit(0,0x58,0xc1); int clean=codegen_block_cycles;
    begin(); emit(0,0x58,0xc1); CHECK(codegen_block_cycles==clean);
    puts("Pentium III SSE hardware anchors and operand dependencies passed");
    return 0;
}
