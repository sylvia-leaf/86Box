CPU correctness, timing and host performance
============================================

This standalone suite needs an x86-64 host, CMake, Ninja and GCC or Clang.
It does not need the GUI, ROMs, a guest OS, Google Test or a guest compiler.
When Python 3 is available, CTest also verifies the performance gate's handling
of stable, noisy, inconsistent and invalid measurements.

  cmake -S tests/cpu/regressions -B build/cpu-regressions -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build build/cpu-regressions
  ctest --test-dir build/cpu-regressions --output-on-failure

simd_exception_test runs raw protected-mode guest instructions with real IDT
handlers. It checks #XM/#UD, fault EIP, unchanged XMM/GPR/MMX/EFLAGS destinations,
MXCSR sticky flags, signed FTZ results, segment-relative alignment, a16/a32 and
register/memory operands. Masks alternate after warming code, using LDMXCSR and
FXRSTOR. Both CPU families, execution engines and FPU implementations run.
On linkers supporting --wrap, masked arithmetic must produce native SSE regions;
disabling native SSE is a failure. sse_fastpath also requires one MXCSR region
and one SSE entry check for adjacent arithmetic. It preserves the memory-helper
rechecks covered by the existing fixture.

Counted MMX loops alternate pending x87 exceptions after warm-up and check
unchanged destinations/tags on #MF as well as carry preservation on success.
IR coverage retains checks at unchecked joins and after memory/general calls,
while sharing checks across integer flag helpers and dominated loop exits.

absolute_address_test checks all scalar absolute MOV emitters, LDMXCSR and
the shared SSE absolute-address path. It decodes effective addresses for
negative/positive displacements, signed 8/32-bit boundaries, offsets beyond
4 GB and low absolute addresses on both sides of 2 GB. It also exercises
allocator block boundaries and executes real loads/stores with memory guards.
The same fixture is built with -O3 and, when supported, LTO. Explicit negative
offsets ensure coverage even when the linker happens to place globals after
cpu_state. A full-core LTO run can additionally be configured with:

  cmake -S tests/cpu/regressions -B build/cpu-lto -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
  cmake --build build/cpu-lto --target opcode_test absolute_address_lto_test
  ctest --test-dir build/cpu-lto --output-on-failure -R "opcodes_|absolute_address_lto"

The deterministic timing checks use independent register dependency chains and
eight independent recurrences for throughput. They check 12 Pentium III and 22
Willamette SIMD/MMX anchors, plus Willamette ADD, IMUL and x87 FADD. SIB dependency
coverage includes every mod/scale/base/index combination. Pending P6 producers
must publish their completion times before consumers are scheduled.

Sources:
  Agner Fog, Instruction tables, Pentium II/III, pp.187-188 (2025-09-20):
  https://www.agner.org/optimize/instruction_tables.pdf
  Intel Pentium 4/Xeon Optimization Reference Manual 248966-004, Appendix C:
  https://kib.kiev.ua/x86docs/Intel/Intel-OptimGuide/248966-004.pdf
  Intel SDM, Vol.1 SIMD exception handling and Vol.2 instruction semantics:
  https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

Timing limits: these are warm scheduling approximations. They do not simulate
cache misses, branch prediction, trace-cache misses, replay, memory disambiguation
or operand-dependent FP assists. Unlisted Willamette integer/system operations
use a four-clock estimate; cache flushes retain the prior conservative estimate.
The existing P6 integer/x87 model still has uncalibrated instructions. The new
model is not a claim of cycle-exact whole-machine emulation, and architectural
timing changes are not host-speed improvements.

For host speed, use the checked fixed-work workload runner. Capture the working
tree BEFORE changing code, including any uncommitted performance improvements:

  python tests/cpu/run_workload_compare.py --output build/cpu-speed --variant before=worktree --build-only

After the change, reuse that immutable executable and build the candidate:

  python tests/cpu/run_workload_compare.py --output build/cpu-speed --variant before=worktree --variant after=worktree --reuse-builds --rounds 6 --samples 9 --sample-ms 75 --fail-on-regression

Alternatively compare two commits with --variant before=REF --variant after=REF.
On Windows, add --cpu N to pin both versions to the same logical core. On Linux,
use taskset. Keep the host idle; do not compile while collecting samples.
Add --interpreter to measure interpreter speed; the default measures the JIT.

All 21 workloads validate guest output in both engines before measurement.
MMX register and load/add/store loops include EMMS and cover both FPU modes,
so entry checks and delayed state updates are included in the speed comparison.
The runner records source/binary hashes, compiler flags, raw samples and paired
round medians. It alternates version order. Positive time_delta_pct means slower.
The gate exits 1 for incorrect guests or an overall slowdown exceeding both 3%
and its MAD-based noise threshold, with more than 3% in every paired round.
Noisy runs or estimated slowdowns above 3% that cannot yet be confirmed exit 2.
Noisy runs must be repeated, not treated as passes. Wall-clock gates are opt-in;
normal CTest uses deterministic correctness and generated-code checks.

These measurements cover synthetic workloads on the measured host, not every
guest application, other host architectures, compilation cost or GUI throughput.
