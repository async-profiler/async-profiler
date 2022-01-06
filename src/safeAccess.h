/*
 * Copyright 2021 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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


class SafeAccess {
  public:
    NOINLINE __attribute__((aligned(16)))
    static void* load(void** ptr) {
        return *ptr;
    }

    static uintptr_t skipFaultInstruction(uintptr_t pc) {
        if (pc - (uintptr_t)load < 16) {
#if defined(__x86_64__)
            return *(u16*)pc == 0x8b48 ? 3 : 0;  // mov rax, [reg]
#elif defined(__i386__)
            return *(u8*)pc == 0x8b ? 2 : 0;     // mov eax, [reg]
#elif defined(__arm__) || defined(__thumb__)
            return (*(instruction_t*)pc & 0x0e50f000) == 0x04100000 ? 4 : 0;  // ldr r0, [reg]
#elif defined(__aarch64__)
            return (*(instruction_t*)pc & 0xffc0001f) == 0xf9400000 ? 4 : 0;  // ldr x0, [reg]
#else
            return sizeof(instruction_t);
#endif
        }
        return 0;
    }
};

#endif // _SAFEACCESS_H
