#include "codegen_backend_x86-64_defs.h"

/* Raised from 0x4000 - too small to hold both status variants (16/32-bit,
   flat/non-flat) of hot code at once, causing recompile thrash - and from
   0x10000, which Windows 2000's boot overran: it evicted and compiled again
   15-30 thousand blocks a second. Block indices are 32-bit. */
#define BLOCK_SIZE  0x20000
#define BLOCK_MASK  0x1ffff
#define BLOCK_START 0

#define HASH_SIZE   0x20000
#define HASH_MASK   0x1ffff

<<<<<<< HEAD
#define HASH(l)     (((l) ^ ((l)>>12)) &0x1ffff)
=======
/* The block hash takes the physical address's bits above 128K too: with the
   low 17 alone, code at the same offset in different 128K regions (the
   kernel's and a driver's, two processes' copies) fought for one slot and
   went to the tree walk each time. */
#define HASH(l)     (((l) ^ ((l) >> 17)) & 0x1ffff)
>>>>>>> 25e5b8173c022dfcb976aebcd4eb3563febdc5ae

#define BLOCK_MAX   0x3c0

#define CODEGEN_BACKEND_HAS_MOV_IMM
<<<<<<< HEAD
#define CODEGEN_BACKEND_HAS_MEM_REGS
#define CODEGEN_BACKEND_HAS_SSE_REGS
#define CODEGEN_BACKEND_HAS_SSE_RECHECK
#define CODEGEN_BACKEND_HAS_SSE_MXCSR
#define CODEGEN_BACKEND_HAS_MEM_STUBS
#define CODEGEN_BACKEND_HAS_MEM_LOOKUP_REUSE
#define CODEGEN_BACKEND_HAS_CMP_ULT
#define CODEGEN_BACKEND_HAS_CMP_SLT
#define CODEGEN_BACKEND_HAS_CMP_Z
#define CODEGEN_BACKEND_HAS_PARITY
#define CODEGEN_BACKEND_HAS_OVERFLOW
#define CODEGEN_BACKEND_HAS_CMOV_Z
#define CODEGEN_BACKEND_HAS_DIVMOD

#ifdef _WIN64
#    define CODEGEN_BACKEND_HAS_SELECTIVE_XMM
/* The allocator records every XMM register loaded or written by this block. */
extern uint16_t codegen_win64_xmm_used;
#endif

#define CODEGEN_HAS_SSE

#define CODEGEN_HOST_CPU_FEATURE_SSE3   (1ULL << 0)
#define CODEGEN_HOST_CPU_FEATURE_SSSE3  (1ULL << 1)
#define CODEGEN_HOST_CPU_FEATURE_SSE4_1 (1ULL << 2)
#define CODEGEN_HOST_CPU_FEATURE_SSE4_2 (1ULL << 3)
#define CODEGEN_HOST_CPU_FEATURE_AVX    (1ULL << 4)
#define CODEGEN_HOST_CPU_FEATURE_AVX2   (1ULL << 5)
#define CODEGEN_HOST_CPU_FEATURE_BMI1   (1ULL << 6)
#define CODEGEN_HOST_CPU_FEATURE_BMI2   (1ULL << 7)
#define CODEGEN_HOST_CPU_FEATURE_AVX512 (1ULL << 8)

extern uint64_t codegen_host_cpu_features;

static inline int
codegen_host_cpu_has_feature(uint64_t feature)
{
    return (codegen_host_cpu_features & feature) == feature;
}
=======
/* The integer uops of the SETcc, BSWAP, BSF/BSR, CMPXCHG and multiply
   handlers: CMP_SET, TEST_SET, BSF, BSR, BSWAP, IMUL, IMUL_HI, IMUL_OV and
   MUL_HI. A backend without them leaves those instructions to the
   interpreter. */
#define CODEGEN_BACKEND_HAS_INT_UOPS
>>>>>>> 25e5b8173c022dfcb976aebcd4eb3563febdc5ae
