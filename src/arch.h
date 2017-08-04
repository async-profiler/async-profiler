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


#if defined(__x86_64__) || defined(__i386__)

#define spinPause()       asm volatile("pause")
#define rmb()             asm volatile("lfence" : : : "memory")
#define flushCache(addr)  asm volatile("mfence; clflush (%0); mfence" : : "r"(addr) : "memory")

typedef unsigned char instruction_t;
const instruction_t BREAKPOINT = 0xcc;

#elif defined(__arm__) || defined(__thumb__)

#define spinPause()       asm volatile("yield")
#define rmb()             asm volatile("dmb ish" : : : "memory")
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)((addr) + 1))

typedef unsigned int instruction_t;
const instruction_t BREAKPOINT = 0xe7f001f0;

#else

#define spinPause()
#define rmb()             __sync_synchronize()
#define flushCache(addr)  __builtin___clear_cache((char*)(addr), (char*)((addr) + 1))

#endif


#endif // _ARCH_H
