/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "safeAccess.h"
#include "stackFrame.h"

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


extern instruction_t load_end[];
extern instruction_t load32_end[];

NOINLINE
void* SafeAccess::load(void** ptr, void* default_value) {
#if defined(__x86_64__)
    void* ret;
    asm volatile("mov (%1), %0" : "=a"(ret) : "r"(ptr), "S"(default_value));
#elif defined(__i386__)
    void* ret;
    asm volatile("mov (%1), %0" : "=a"(ret) : "r"(ptr), "a"(default_value));
#elif defined(__aarch64__)
    register void* ret asm("x0");
    asm volatile("ldr %0, [%1]" : "=r"(ret) : "r"(ptr), "r"(default_value));
#else
    asm volatile("" : : "r"(default_value));  // prevent compiler from optimizing the argument away
    void* ret = *ptr;
#endif
    LABEL(load_end);
    return ret;
}

NOINLINE
int32_t SafeAccess::load32(int32_t* ptr, int32_t default_value) {
#if defined(__x86_64__)
    int32_t ret;
    asm volatile("movl (%1), %0" : "=a"(ret) : "r"(ptr), "S"(default_value));
#elif defined(__i386__)
    int32_t ret;
    asm volatile("movl (%1), %0" : "=a"(ret) : "r"(ptr), "a"(default_value));
#elif defined(__aarch64__)
    register int32_t ret asm("w0");
    asm volatile("ldr %w0, [%1]" : "=r"(ret) : "r"(ptr), "r"(default_value));
#else
    asm volatile("" : : "r"(default_value));  // prevent compiler from optimizing the argument away
    int32_t ret = *ptr;
#endif
    LABEL(load32_end);
    return ret;
}

// When a memory access error happens in SafeAccess::load/load32,
// this function skips the fault instruction pretending it has loaded default_value
bool SafeAccess::checkFault(StackFrame& frame) {
    instruction_t* pc = (instruction_t*)frame.pc();
    if (!(pc >= (void*)load && pc < load_end) &&
        !(pc >= (void*)load32 && pc < load32_end)) {
        return false;
    }

#if defined(__x86_64__)
    // 2 bytes: mov eax, [reg] OR 3 bytes: mov rax, [reg]
    frame.pc() += pc[0] == 0x8b ? 2 : 3;
    frame.retval() = frame.arg1();
#elif defined(__i386__)
    // eax already holds default_value
    frame.pc() += 2;
#else
    frame.pc() += sizeof(instruction_t);
    frame.retval() = frame.arg1();
#endif

    return true;
}
