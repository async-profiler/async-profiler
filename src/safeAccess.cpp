/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "safeAccess.h"

#ifdef __clang__
#  define NOINLINE __attribute__((noinline))
#else
#  define NOINLINE __attribute__((noinline,noclone))
#endif

#ifdef __APPLE__
#  define LABEL(sym) asm volatile(".globl _" #sym "\n_" #sym ":")
#else
#  define LABEL(sym) asm volatile(".globl " #sym "\n" #sym ":")
#endif

// Ensure that compiler passes the argument, i.e. assigns the corresponding input register
#define CONSUME_ARG(arg) asm volatile("" : : "r"(arg))


extern instruction_t load_end[];
extern instruction_t load32_end[];

NOINLINE
void* SafeAccess::load(void** ptr, void* default_value) {
    CONSUME_ARG(default_value);
    void* ret = *ptr;
    LABEL(load_end);
    return ret;
}

NOINLINE
u32 SafeAccess::load32(u32* ptr, u32 default_value) {
    CONSUME_ARG(default_value);
    u32 ret = *ptr;
    LABEL(load32_end);
    return ret;
}

// When a memory access error happens in SafeAccess::load/load32,
// this function returns how many bytes to skip to advance to the next instruction
u32 SafeAccess::skipLoad(instruction_t* pc) {
    if ((pc >= (void*)load && pc < load_end) ||
        (pc >= (void*)load32 && pc < load32_end)) {
#if defined(__x86_64__) || defined(__i386__)
        if (pc[0] == 0x8b) return 2;                   // mov eax, [reg]
        if (pc[0] == 0x48 && pc[1] == 0x8b) return 3;  // mov rax, [reg]
#else
        return sizeof(instruction_t);
#endif
    }
    return 0;
}
