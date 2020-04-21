/*
 * Copyright 2017 Andrei Pangin
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

#ifndef _ARCH_H
#define _ARCH_H


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

static inline u64 atomicInc(volatile u64& var, u64 increment = 1) {
    return __sync_fetch_and_add(&var, increment);
}

static inline int atomicInc(volatile int& var, int increment = 1) {
    return __sync_fetch_and_add(&var, increment);
}


#if defined(__x86_64__) || defined(__i386__)

typedef unsigned char instruction_t;
const instruction_t BREAKPOINT = 0xcc;

const int SYSCALL_SIZE = 2;
const int PLT_HEADER_SIZE = 16;
const int PLT_ENTRY_SIZE = 16;
const int PERF_REG_PC = 8;  // PERF_REG_X86_IP

#define spinPause()       asm volatile("pause")
#define rmb()             asm volatile("lfence" : : : "memory")
#define flushCache(addr)  asm volatile("mfence; clflush (%0); mfence" : : "r"(addr) : "memory")

#elif defined(__arm__) || defined(__thumb__)

typedef unsigned int instruction_t;
const instruction_t BREAKPOINT = 0xe7f001f0;

const int SYSCALL_SIZE = sizeof(instruction_t);
const int PLT_HEADER_SIZE = 20;
const int PLT_ENTRY_SIZE = 12;
const int PERF_REG_PC = 15;  // PERF_REG_ARM_PC

#define spinPause()       asm volatile("yield")
#define rmb()             asm volatile("dmb ish" : : : "memory")
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)(addr) + sizeof(instruction_t))

#elif defined(__aarch64__)

typedef unsigned int instruction_t;
const instruction_t BREAKPOINT = 0xd4200000;

const int SYSCALL_SIZE = sizeof(instruction_t);
const int PLT_HEADER_SIZE = 32;
const int PLT_ENTRY_SIZE = 16;
const int PERF_REG_PC = 32;  // PERF_REG_ARM64_PC

#define spinPause()       asm volatile("yield")
#define rmb()             asm volatile("dmb ish" : : : "memory")
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)(addr) + sizeof(instruction_t))

#else
#warning "Compiling on unsupported arch"

#define spinPause()
#define rmb()             __sync_synchronize()
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)(addr) + sizeof(instruction_t))

#endif


#endif // _ARCH_H
