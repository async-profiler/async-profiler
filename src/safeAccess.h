/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SAFEACCESS_H
#define _SAFEACCESS_H

#include <stdint.h>
#include "arch.h"


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

class SafeAccess {
  public:
    NOINLINE
    static void* load(void** ptr, void* default_value = NULL) {
        void* ret = *ptr;
        LABEL(load_end);
        return ret;
    }

    NOINLINE
    static u32 load32(u32* ptr, u32 default_value = 0) {
        u32 ret = *ptr;
        LABEL(load32_end);
        return ret;
    }

    static uintptr_t skipLoad(instruction_t* pc) {
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
};

#endif // _SAFEACCESS_H
