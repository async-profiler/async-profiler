/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ARCH_H
#define _ARCH_H


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

static inline u64 atomicInc(volatile u64& var, u64 increment = 1) {
    return __sync_fetch_and_add(&var, increment);
}

static inline int atomicInc(volatile u32& var, int increment = 1) {
    return __sync_fetch_and_add(&var, increment);
}

static inline int atomicInc(volatile int& var, int increment = 1) {
    return __sync_fetch_and_add(&var, increment);
}

static inline u64 loadAcquire(u64& var) {
    return __atomic_load_n(&var, __ATOMIC_ACQUIRE);
}

static inline void storeRelease(u64& var, u64 value) {
    return __atomic_store_n(&var, value, __ATOMIC_RELEASE);
}


#if defined(__x86_64__) || defined(__i386__)

typedef unsigned char instruction_t;
const instruction_t BREAKPOINT = 0xcc;
const int BREAKPOINT_OFFSET = 0;

const int SYSCALL_SIZE = 2;
const int FRAME_PC_SLOT = 1;
const int PROBE_SP_LIMIT = 4;
const int PLT_HEADER_SIZE = 16;
const int PLT_ENTRY_SIZE = 16;
const int PERF_REG_PC = 8;  // PERF_REG_X86_IP

#define spinPause()       asm volatile("pause")
#define rmb()             asm volatile("lfence" : : : "memory")
#define flushCache(addr)  asm volatile("mfence; clflush (%0); mfence" : : "r" (addr) : "memory")

#elif defined(__arm__) || defined(__thumb__)

typedef unsigned int instruction_t;
const instruction_t BREAKPOINT = 0xe7f001f0;
const instruction_t BREAKPOINT_THUMB = 0xde01de01;
const int BREAKPOINT_OFFSET = 0;

const int SYSCALL_SIZE = sizeof(instruction_t);
const int FRAME_PC_SLOT = 1;
const int PROBE_SP_LIMIT = 0;
const int PLT_HEADER_SIZE = 20;
const int PLT_ENTRY_SIZE = 12;
const int PERF_REG_PC = 15;  // PERF_REG_ARM_PC

#define spinPause()       asm volatile("yield")
#define rmb()             asm volatile("dmb ish" : : : "memory")
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)(addr) + sizeof(instruction_t))

#elif defined(__aarch64__)

typedef unsigned int instruction_t;
const instruction_t BREAKPOINT = 0xd4200000;
const int BREAKPOINT_OFFSET = 0;

const int SYSCALL_SIZE = sizeof(instruction_t);
const int FRAME_PC_SLOT = 1;
const int PROBE_SP_LIMIT = 0;
const int PLT_HEADER_SIZE = 32;
const int PLT_ENTRY_SIZE = 16;
const int PERF_REG_PC = 32;  // PERF_REG_ARM64_PC

#define spinPause()       asm volatile("isb")
#define rmb()             asm volatile("dmb ish" : : : "memory")
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)(addr) + sizeof(instruction_t))

#elif defined(__PPC64__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

typedef unsigned int instruction_t;
const instruction_t BREAKPOINT = 0x7fe00008;
// We place the break point in the third instruction slot on PPCLE as the first two are skipped if
// the call comes from within the same compilation unit according to the LE ABI.
const int BREAKPOINT_OFFSET = 8;

const int SYSCALL_SIZE = sizeof(instruction_t);
const int FRAME_PC_SLOT = 2;
const int PROBE_SP_LIMIT = 0;
const int PLT_HEADER_SIZE = 24;
const int PLT_ENTRY_SIZE = 24;
const int PERF_REG_PC = 32;  // PERF_REG_POWERPC_NIP

#define spinPause()       asm volatile("yield") // does nothing, but using or 1,1,1 would lead to other problems
#define rmb()             asm volatile ("sync" : : : "memory") // lwsync would do but better safe than sorry
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)(addr) + sizeof(instruction_t))

#elif defined(__riscv) && (__riscv_xlen == 64)

typedef unsigned int instruction_t;
#if defined(__riscv_compressed)
const instruction_t BREAKPOINT = 0x9002; // EBREAK (compressed form)
#else
const instruction_t BREAKPOINT = 0x00100073; // EBREAK
#endif
const int BREAKPOINT_OFFSET = 0;

const int SYSCALL_SIZE = sizeof(instruction_t);
const int FRAME_PC_SLOT = 1;    // return address is at -1 from FP
const int PROBE_SP_LIMIT = 0;
const int PLT_HEADER_SIZE = 24; // Best guess from examining readelf
const int PLT_ENTRY_SIZE = 24;  // ...same...
const int PERF_REG_PC = 0;      // PERF_REG_RISCV_PC

#define spinPause()       // No architecture support
#define rmb()             asm volatile ("fence" : : : "memory")
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)(addr) + sizeof(instruction_t))

#elif defined(__loongarch_lp64)

typedef unsigned int instruction_t;
const instruction_t BREAKPOINT = 0x002a0005; // EBREAK
const int BREAKPOINT_OFFSET = 0;

const int SYSCALL_SIZE = sizeof(instruction_t);
const int FRAME_PC_SLOT = 1;
const int PROBE_SP_LIMIT = 0;
const int PLT_HEADER_SIZE = 32;
const int PLT_ENTRY_SIZE = 16;
const int PERF_REG_PC = 0;      // PERF_REG_LOONGARCH_PC

#define spinPause()       asm volatile("ibar 0x0")
#define rmb()             asm volatile("dbar 0x0" : : : "memory")
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)(addr) + sizeof(instruction_t))

#else

#error "Compiling on unsupported arch"

#endif


// Return address signing support.
// Apple M1 has 47 bit virtual addresses.
#if defined(__aarch64__) && defined(__APPLE__)
#  define ADDRESS_BITS 47
#  define WX_MEMORY    true
#else
#  define WX_MEMORY    false
#endif

#ifdef ADDRESS_BITS
static inline const void* stripPointer(const void* p) {
    return (const void*) ((unsigned long)p & ((1UL << ADDRESS_BITS) - 1));
}
#else
#  define stripPointer(p)  (p)
#endif


#endif // _ARCH_H
