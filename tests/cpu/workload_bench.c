/* Fixed-work guest kernels through the production decoder, MMU and JIT.
 * Every invocation contributes to a checked checksum. The completion OUT
 * ends the host slice; no idle guest loop is included in the measurement. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#    include <windows.h>
#endif
#include <86box/86box.h>
#include <86box/mem.h>
#include <86box/timer.h>
#include "cpu.h"
#include "x86.h"
#include "x86seg.h"
#include "codegen.h"

extern int32_t cycles_main;
extern int x86_was_reset, inrecomp;
enum { DRIVER = 0x10000, BODY = 0x11000, DATA = 0x600000,
       DEST = 0x610000, CALLS = 256, MAX_SAMPLES = 99 };
enum { ADD, IMUL, ADC, SBB, SUM, COPY, REP, SSE_REGISTER, SSE_MEM, X87, MMX_REGISTER, MMX_MEM };
typedef struct {
    const char *name;
    unsigned kind, count, offset, paged, softfloat;
} workload;
static const workload cases[] = {
    { "integer/add-32", ADD, 32 },
    { "integer/imul-32", IMUL, 32 },
    { "carry/adc-2", ADC, 2 },
    { "carry/adc-32", ADC, 32 },
    { "carry/sbb-2", SBB, 2 },
    { "carry/sbb-32", SBB, 32 },
    { "memory/sum-256", SUM, 256 },
    { "memory/sum-256-unaligned", SUM, 256, 1 },
    { "memory/sum-256-paged", SUM, 256, 0, 1 },
    { "memory/copy-256", COPY, 256 },
    { "rep/movsd-256", REP, 256 },
    { "rep/movsd-4096", REP, 4096 },
    { "sse/addps-32", SSE_REGISTER, 32 },
    { "sse/addps-128", SSE_REGISTER, 128 },
    { "sse/load-add-store-256", SSE_MEM, 256 },
    { "x87/fadd-64-fast", X87, 64 },
    { "x87/fadd-64-soft", X87, 64, 0, 0, 1 },
    { "mmx/paddd-32", MMX_REGISTER, 32 },
    { "mmx/paddd-128", MMX_REGISTER, 128 },
    { "mmx/load-add-store-128", MMX_MEM, 128 },
    { "mmx/paddd-128-soft", MMX_REGISTER, 128, 0, 0, 1 },
};
static const workload *active;
static uint32_t expected, checksum;
static uint8_t expected_memory[16384];
static unsigned result_bytes, compiled_batches, batches;
static int done, interpreter;
static uint8_t *emit;
uint64_t timer_target = 1000;

void fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fprintf(stderr, "\n%s pc=%08x abort=%u reset=%d\n",
            active ? active->name : "setup", cpu_state.pc, cpu_state.abrt, x86_was_reset);
    exit(2);
}
void pclog(const char *fmt, ...) { (void) fmt; }
void pclog_ex(const char *fmt, va_list ap) { (void) fmt; (void) ap; }
void outb(uint16_t p, uint8_t v) { fatal("Unexpected OUT %x=%x", p, v); }
void outw(uint16_t p, uint16_t v) { fatal("Unexpected OUT %x=%x", p, v); }
uint8_t inb(uint16_t p) { fatal("Unexpected IN %x", p); return 0; }
uint16_t inw(uint16_t p) { fatal("Unexpected IN %x", p); return 0; }
uint32_t inl(uint16_t p) { fatal("Unexpected IN %x", p); return 0; }
void outl(uint16_t p, uint32_t v)
{
    if (p != 0xe9 || done || v != expected * (uint32_t) CALLS)
        fatal("Incorrect checksum: port=%x got=%08x expected=%08x", p, v, expected * (uint32_t) CALLS);
    checksum = v;
    done = 1;
    compiled_batches += !!inrecomp;
    ++batches;
    cycles = cycles_main = 0;
    cpu_block_end = 1;
}
void timer_process(void)
{
    timer_target = tsc + 1000;
    if (cpu_state.abrt || x86_was_reset)
        fatal("Unexpected guest exception/reset");
}
static double now_ns(void)
{
#ifdef _WIN32
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t); QueryPerformanceFrequency(&f);
    return (double) t.QuadPart * 1e9 / (double) f.QuadPart;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double) t.tv_sec * 1e9 + t.tv_nsec;
#endif
}
static void emit8(uint8_t v) { *emit++ = v; }
static void word32(uint32_t v) { memcpy(emit, &v, 4); emit += 4; }
static void mov(unsigned reg, uint32_t v) { emit8(0xb8 + reg); word32(v); }
static void loop(uint8_t *start, int signed_count)
{
    emit8(0x49); /* DEC ECX preserves carry. */
    emit8(signed_count ? 0x7d : 0x75); /* JGE / JNZ */
    ptrdiff_t displacement = start - (emit + 1);
    if (displacement < -128 || displacement > 127) fatal("Loop too large");
    emit8((uint8_t) displacement);
}
static void flat(x86seg *s, unsigned selector, unsigned access)
{
    *s = (x86seg) { .base = 0, .limit = UINT32_MAX, .access = access,
        .ar_high = 0xcf, .seg = selector, .limit_low = 0,
        .limit_high = UINT32_MAX, .checked = 1 };
}
static void prepare(const workload *c)
{
    active = c;
    fpu_softfloat = c->softfloat;
    cpu_set();
    resetx86(); codegen_reset();
    cr0 = 1; cr4 = CR4_OSFXSR | CR4_OSXMMEXCPT;
    flat(&cpu_state.seg_cs, 8, 0x9b);
    flat(&cpu_state.seg_ds, 16, 0x93); flat(&cpu_state.seg_es, 16, 0x93);
    flat(&cpu_state.seg_ss, 16, 0x93); flat(&cpu_state.seg_fs, 16, 0x93);
    flat(&cpu_state.seg_gs, 16, 0x93);
    cpu_cur_status = CPU_STATUS_PMODE | CPU_STATUS_USE32 | CPU_STATUS_STACK32;
    use32 = 0x300; stack32 = 1;
    cpu_state.flags = 2; cpu_state.eflags = 0;
    cpu_state.npxc = 0x37f; cpu_state.mxcsr = 0x1f80;
    cycles = cycles_main = 0; tsc = 0; timer_target = 1000;
    x86_was_reset = 0; mem_a20_alt = 1; mem_a20_recalc();
    if (c->paged) {
        uint32_t *pt = (uint32_t *) (ram + 0x800000);
        uint32_t *pd = (uint32_t *) (ram + 0x804000);
        memset(pd, 0, 4096);
        for (unsigned i = 0; i < 4096; ++i) pt[i] = i * 4096 + 3;
        for (unsigned i = 0; i < 4; ++i) pd[i] = 0x800003 + i * 4096;
        cr3 = 0x804000; cr0 |= 0x80000000;
    }
    flushmmucache();
    memset(ram + DEST - 16, 0xa5, sizeof(expected_memory) + 32);
    for (unsigned i = 0; i < 4096; ++i) {
        uint32_t v = i * 17 + 3;
        memcpy(ram + DATA + c->offset + i * 4, &v, 4);
    }
    result_bytes = compiled_batches = batches = expected = 0;
    emit = ram + DRIVER;
    if (c->kind == X87) { emit8(0xdb); emit8(0xe3); } /* FNINIT, once per batch. */
    mov(7, CALLS); emit8(0x31); emit8(0xdb); /* EDI=count; XOR EBX,EBX */
    uint8_t *call = emit;
    emit8(0xe8); word32(BODY - (DRIVER + (unsigned) (emit - (ram + DRIVER)) + 4));
    emit8(0x01); emit8(0xc3); /* ADD EBX,EAX */
    emit8(0x4f); emit8(0x75); emit8((uint8_t) (call - (emit + 1)));
    emit8(0x89); emit8(0xd8); emit8(0xe7); emit8(0xe9); /* MOV EAX,EBX; OUT e9,EAX */
    emit8(0xeb); emit8(0xfe); /* Defensive tail, never intentionally timed. */
    emit = ram + BODY;
    mov(1, c->count - (c->kind == ADC || c->kind == SBB));
    if (c->kind == ADC) { mov(0, UINT32_MAX); emit8(0x31); emit8(0xd2); }
    else if (c->kind == SBB) { emit8(0x31); emit8(0xc0); }
    else mov(0, c->kind == IMUL ? 1 : 0);
    mov(6, DATA + c->offset); mov(2, DEST);
    if (c->kind == SSE_REGISTER || c->kind == SSE_MEM) {
        for (unsigned i = 0; i < 4; ++i) {
            float v = (float) (i + 1);
            memcpy(ram + DATA + i * 4, &v, 4);
            memcpy(ram + DATA + 16 + i * 4, &v, 4);
        }
        emit8(0x0f); emit8(0x10); emit8(0x06); /* MOVUPS XMM0,[ESI] */
        emit8(0x0f); emit8(0x10); emit8(0x4e); emit8(16); /* MOVUPS XMM1,[ESI+16] */
    }
    if (c->kind == X87) {
        float one = 1;
        memcpy(ram + DATA, &one, 4);
        emit8(0xd9); emit8(0xee); /* FLDZ */
    }
    if (c->kind == MMX_REGISTER || c->kind == MMX_MEM) {
        for (unsigned i = 0; i < 2; ++i) {
            uint32_t v = i + 1;
            memcpy(ram + DATA + i * 4, &v, 4);
            memcpy(ram + DATA + 8 + i * 4, &v, 4);
        }
        emit8(0x0f); emit8(0x6f); emit8(0x06); /* MOVQ MM0,[ESI] */
        emit8(0x0f); emit8(0x6f); emit8(0x4e); emit8(8); /* MOVQ MM1,[ESI+8] */
    }
    uint8_t *start = emit;
    switch (c->kind) {
        case ADD: case ADC: case SBB:
            emit8(0x83); emit8(c->kind == ADD ? 0xc0 : c->kind == ADC ? 0xd0 : 0xd8);
            emit8(c->kind == ADD ? 3 : 1);
            expected = c->kind == ADD ? c->count * 3 : c->kind == ADC ? c->count : 0u - c->count - 1;
            break;
        case IMUL:
            emit8(0x69); emit8(0xc0); word32(1664525);
            emit8(0x05); word32(1013904223);
            expected = 1;
            for (unsigned i = 0; i < c->count; ++i) expected = expected * 1664525u + 1013904223u;
            break;
        case SUM:
            emit8(0x03); emit8(0x06); /* ADD EAX,[ESI] */
            emit8(0x83); emit8(0xc6); emit8(4);
            for (unsigned i = 0; i < c->count; ++i) expected += i * 17 + 3;
            break;
        case COPY:
            emit8(0x8b); emit8(0x06); emit8(0x89); emit8(0x02);
            emit8(0x83); emit8(0xc6); emit8(4); emit8(0x83); emit8(0xc2); emit8(4);
            break;
        case REP:
            emit8(0x57); mov(7, DEST); emit8(0xfc); emit8(0xf3); emit8(0xa5); emit8(0x5f);
            emit8(0xa1); word32(DEST + (c->count - 1) * 4);
            break;
        case SSE_REGISTER: case SSE_MEM:
            if (c->kind == SSE_MEM) { emit8(0x0f); emit8(0x10); emit8(0x06); }
            emit8(0x0f); emit8(0x58); emit8(0xc1); /* ADDPS XMM0,XMM1 */
            if (c->kind == SSE_MEM) { emit8(0x0f); emit8(0x11); emit8(0x02); }
            break;
        case X87: emit8(0xd8); emit8(0x06); break; /* FADD m32 */
        case MMX_REGISTER: case MMX_MEM:
            if (c->kind == MMX_MEM) { emit8(0x0f); emit8(0x6f); emit8(0x06); }
            emit8(0x0f); emit8(0xfe); emit8(0xc1); /* PADDD MM0,MM1 */
            if (c->kind == MMX_MEM) { emit8(0x0f); emit8(0x7f); emit8(0x02); }
            break;
    }
    if (c->kind != REP) loop(start, c->kind == ADC || c->kind == SBB);
    if (c->kind == SSE_REGISTER || c->kind == SSE_MEM) {
        emit8(0x0f); emit8(0x11); emit8(0x02);
        result_bytes = 16;
        for (unsigned i = 0; i < 4; ++i) {
            float v = (float) ((i + 1) * (c->kind == SSE_MEM ? 2 : c->count + 1));
            memcpy(expected_memory + 4 * i, &v, 4);
        }
    } else if (c->kind == MMX_REGISTER || c->kind == MMX_MEM) {
        emit8(0x0f); emit8(0x7f); emit8(0x02); /* MOVQ [EDX],MM0 */
        emit8(0x0f); emit8(0x77); /* EMMS */
        result_bytes = 8;
        for (unsigned i = 0; i < 2; ++i) {
            uint32_t v = (i + 1) * (c->kind == MMX_MEM ? 2 : c->count + 1);
            memcpy(expected_memory + 4 * i, &v, 4);
        }
    } else if (c->kind == X87) {
        emit8(0xd9); emit8(0x1a); /* FSTP m32 */
        float v = (float) c->count;
        memcpy(expected_memory, &v, 4); result_bytes = 4;
    } else if (c->kind == COPY || c->kind == REP) {
        result_bytes = c->count * 4;
        memcpy(expected_memory, ram + DATA, result_bytes);
        expected = (c->count - 1) * 17 + 3;
    }
    if (c->kind == SSE_REGISTER || c->kind == SSE_MEM || c->kind == X87 || c->kind == MMX_REGISTER || c->kind == MMX_MEM) {
        emit8(0xa1); word32(DEST);
        memcpy(&expected, expected_memory, 4);
    }
    emit8(0xc3);
    if (emit > ram + BODY + 256) fatal("Kernel too large");
}
static void run_batch(void)
{
    done = 0; cpu_state.pc = DRIVER; ESP = 0x700000;
    cycles = cycles_main = 0;
    for (unsigned n = 0; !done; ++n) {
        if (n == 10000) fatal("Guest did not finish");
        if (interpreter) exec386(100000); else exec386_dynarec(100000);
    }
    if (ESP != 0x700000 || cpu_state.abrt || x86_was_reset)
        fatal("Incorrect stack/exception state");
}
static void validate_memory(void)
{
    if (result_bytes && memcmp(ram + DEST, expected_memory, result_bytes))
        fatal("Incorrect guest memory result");
    for (unsigned i = 0; i < 16; ++i)
        if (ram[DEST - 16 + i] != 0xa5 || ram[DEST + result_bytes + i] != 0xa5)
            fatal("Guest overwrote a guard byte");
}
static unsigned number(const char *s)
{
    char *end;
    unsigned long n = strtoul(s, &end, 10);
    if (!*s || *end || n > 10000) fatal("Invalid number %s", s);
    return (unsigned) n;
}
int main(int argc, char **argv)
{
    const char *name = NULL, *csv_path = NULL;
    unsigned samples = 9, sample_ms = 75, cpu_index = 0;
    int validate = 0, pin = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--list")) {
            for (unsigned n = 0; n < sizeof(cases) / sizeof(cases[0]); ++n) puts(cases[n].name);
            return 0;
        } else if (!strcmp(argv[i], "--case") && i + 1 < argc) name = argv[++i];
        else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
        else if (!strcmp(argv[i], "--samples") && i + 1 < argc) samples = number(argv[++i]);
        else if (!strcmp(argv[i], "--sample-ms") && i + 1 < argc) sample_ms = number(argv[++i]);
        else if (!strcmp(argv[i], "--cpu") && i + 1 < argc) { cpu_index = number(argv[++i]); pin = 1; }
        else if (!strcmp(argv[i], "--validate-only")) validate = 1;
        else if (!strcmp(argv[i], "--interpreter")) interpreter = 1;
        else fatal("Unknown/incomplete argument %s", argv[i]);
    }
    if (!samples || samples > MAX_SAMPLES || !sample_ms || sample_ms > 1000)
        fatal("Samples must be 1..99 and sample-ms 1..1000");
#ifdef _WIN32
    if (pin && (cpu_index >= sizeof(DWORD_PTR) * 8 ||
                !SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR) 1 << cpu_index)))
        fatal("Cannot pin to CPU %u", cpu_index);
#else
    if (pin) fatal("--cpu currently supports Windows; use taskset on Linux");
#endif
    cpu_f = cpu_get_family("pentium3_katmai"); cpu = -1;
    for (int i = 0; cpu_f && cpu_f->cpus[i].cpu_type; ++i)
        if (cpu_f->cpus[i].rspeed == 500000000) { cpu = i; break; }
    if (cpu < 0) fatal("Pentium III Katmai 500 MHz missing");
    fpu_type = FPU_INTERNAL; cpu_use_dynarec = !interpreter;
    cpu_set(); codegen_init(); mem_size = 16 * 1024; mem_init(); mem_reset();
    FILE *csv = csv_path ? fopen(csv_path, "w") : NULL;
    if (csv_path && !csv) fatal("Cannot open %s", csv_path);
    if (csv) fprintf(csv, "case,sample,calls,kernel_iterations,elapsed_ns,ns_per_call,checksum\n");
    unsigned selected = 0;
    for (unsigned n = 0; n < sizeof(cases) / sizeof(cases[0]); ++n) {
        const workload *c = &cases[n];
        if (name && strcmp(name, c->name)) continue;
        ++selected; prepare(c);
        for (unsigned k = 0; k < 4; ++k) { run_batch(); validate_memory(); }
        if (!interpreter && !compiled_batches) fatal("No compiled completion blocks");
        printf("PASS %s checksum=%08x compiled=%u/%u\n", c->name, checksum, compiled_batches, batches);
        if (validate) continue;
        double start = now_ns();
        do { run_batch(); } while (now_ns() - start < 1e8);
        for (unsigned s = 0; s < samples; ++s) {
            uint64_t calls = 0;
            start = now_ns();
            double elapsed;
            do { run_batch(); calls += CALLS; elapsed = now_ns() - start; }
            while (elapsed < sample_ms * 1e6);
            validate_memory();
            if (csv) fprintf(csv, "%s,%u,%llu,%u,%.0f,%.9f,%08x\n", c->name, s,
                (unsigned long long) calls, c->count, elapsed, elapsed / calls, checksum);
        }
    }
    if (!selected) fatal("No matching case");
    if (csv && fclose(csv)) fatal("Cannot finish CSV");
    puts("All selected workloads validated.");
    return 0;
}
