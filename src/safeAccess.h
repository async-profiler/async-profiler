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
#  define DEFINE_LABEL(sym) asm volatile(".globl _" #sym "\n_" #sym ":")
#else
#  define DEFINE_LABEL(sym) asm volatile(".globl " #sym "\n" #sym ":")
#endif

extern instruction_t sa_load_start[];
extern instruction_t sa_load_end[];
extern instruction_t sa_load32_start[];
extern instruction_t sa_load32_end[];

class SafeAccess {
  public:
    NOINLINE
    static void* load(void** ptr, void* default_value = NULL) {
        DEFINE_LABEL(sa_load_start);
        void* ret = *ptr;
        DEFINE_LABEL(sa_load_end);
        return ret;
    }

    NOINLINE
    static u32 load32(u32* ptr, u32 default_value = 0) {
        DEFINE_LABEL(sa_load32_start);
        u32 ret = *ptr;
        DEFINE_LABEL(sa_load32_end);
        return ret;
    }

    static uintptr_t skipLoad(instruction_t* pc) {
        if ((pc >= sa_load_start && pc < sa_load_end) ||
            (pc >= sa_load32_start && pc < sa_load32_end)) {
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
