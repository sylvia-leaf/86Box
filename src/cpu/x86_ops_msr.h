static int
opRDTSC(UNUSED(uint32_t fetchdat))
{
    if (!cpu_has_feature(CPU_FEATURE_RDTSC)) {
        cpu_state.pc = cpu_state.oldpc;
        x86illegal();
        return 1;
    }
    if ((cr4 & CR4_TSD) && CPL) {
        x86gpf("RDTSC when TSD set and CPL != 0", 0);
        return 1;
    }
    EAX = tsc & 0xffffffff;
    EDX = tsc >> 32;
    CLOCK_CYCLES(1);
#ifdef USE_DYNAREC
    /*
       A due timer run here can reset the CPU (the 8042 pulse); the nonzero
       return ends the recompiled block, as the OUT ops do.
     */
    if (cpu_use_dynarec) {
        update_tsc();
        return x86_was_reset;
    }
#endif
    return 0;
}

static int
opRDPMC(UNUSED(uint32_t fetchdat))
{
    /* Willamette has eighteen counters; bit 31 requests a fast low-dword
       read. P6 exposes two counters and does not support that selector bit. */
    int p4 = cpu_s->cpu_type == CPU_PENTIUM4W;
    uint32_t counter = p4 ? ECX & 0x7fffffff : ECX;
    if (counter >= (p4 ? 18u : 2u) || (!(cr4 & CR4_PCE) && (cr0 & 1) && CPL)) {
        x86gpf("RDPMC not allowed", 0);
        return 1;
    }
    EAX = EDX = 0;
    CLOCK_CYCLES(1);
    return 0;
}
