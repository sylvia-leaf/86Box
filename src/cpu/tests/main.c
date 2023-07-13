#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#ifndef INFINITY
#    define INFINITY (__builtin_inff())
#endif

#include <86box/86box.h>
#include "cpu.h"
#include <86box/timer.h>
#include "x86.h"
#include "x86_ops.h"
#include "x87.h"
#include "x86_flags.h"
#include <86box/dma.h>
#include <86box/io.h>
#include <86box/mem.h>
#include "386_common.h"
#include <86box/nmi.h>
#include <86box/pci.h>
#include <86box/pic.h>
#include <86box/gdbstub.h>
#include "codegen.h"

uint8_t* testram = NULL;
const uint32_t ramsize = 0x1000000;

mem_mapping_t* ram_map = NULL;

uint8_t read_mem_byte(uint32_t addr, void* p)
{
    if(addr < ramsize) return testram[addr];
    else return 0xff;
}

uint16_t read_mem_word(uint32_t addr, void* p)
{
    return read_mem_byte(addr, p) | (read_mem_byte(addr+1, p) << 8);
}

uint32_t read_mem_long(uint32_t addr, void* p)
{
    return read_mem_word(addr, p) | (read_mem_word(addr+2, p) << 16);
}

void write_mem_byte(uint32_t addr, uint8_t val, void* p)
{
    if(addr < ramsize) testram[addr] = val;
}

void write_mem_word(uint32_t addr, uint16_t val, void* p)
{
    write_mem_byte(addr, val & 0xff, p);
    write_mem_byte(addr + 1, val >> 8, p);
}

void write_mem_long(uint32_t addr, uint32_t val, void* p)
{
    write_mem_word(addr, val & 0xffff, p);
    write_mem_word(addr + 2, val >> 16, p);
}

int main(int ac, char** av)
{
    io_init();
    timer_init();

    cpu_dynamic_switch(CPU_GENERICINTEL);

    resetx86();
    dma_reset();
    pci_pic_reset();
    cpu_cache_int_enabled = cpu_cache_ext_enabled = 0;
    cycles = 0;

    testram = malloc(ramsize); //16 MB, plenty to work with for this.
    mem_init();

    mem_mapping_add(ram_map, 0, 0xffffffff, read_mem_byte, read_mem_word, read_mem_long, write_mem_byte, write_mem_word, write_mem_long, NULL, MEM_MAPPING_EXTERNAL, NULL);

    testram[0xffff0] = 0xea; // JMP FAR F000:0000
    testram[0xffff1] = 0x00;
    testram[0xffff2] = 0x00;
    testram[0xffff3] = 0x00;
    testram[0xffff4] = 0xf0;

    setr16(BX, 0x1000);
    testram[0xf0000] = 0x2e; // MOVUPS XMM0, CS:[BX]
    testram[0xf0001] = 0x0f;
    testram[0xf0002] = 0x10;
    testram[0xf0003] = 0x07;
    testram[0xf0004] = 0xf4; // HLT

    testram[0xf1000] = 0x00;
    testram[0xf1001] = 0x01;
    testram[0xf1002] = 0x02;
    testram[0xf1003] = 0x03;
    testram[0xf1004] = 0x04;
    testram[0xf1005] = 0x05;
    testram[0xf1006] = 0x06;
    testram[0xf1007] = 0x07;
    testram[0xf1008] = 0x08;
    testram[0xf1009] = 0x09;
    testram[0xf100a] = 0x0a;
    testram[0xf100b] = 0x0b;
    testram[0xf100c] = 0x0c;
    testram[0xf100d] = 0x0d;
    testram[0xf100e] = 0x0e;
    testram[0xf100f] = 0x0f;

    while(opcode != 0xF4)
    {
        fetchdat = fastreadl_fetch(cs + cpu_state.pc);
    
        opcode = fetchdat & 0xFF;
        fetchdat >>= 8;

        x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);
        sse_xmm = 0;
    }

    if(XMM[0].q[0] == 0x07060504030201ull && XMM[0].q[1] == 0x0f0e0d0c0b0a0908ull)
        printf("MOVUPS matched expected results\n");
    else
        printf("MOVUPS did not match expected results! XMM low half %" PRIx64" XMM high half %"PRIx64"\n", XMM[0].q[0], XMM[0].q[1]);

    free(testram);
    mem_close();
    timer_close();
    cpu_close();
}