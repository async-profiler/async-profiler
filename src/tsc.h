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

#ifndef _TSC_H
#define _TSC_H

#include "os.h"


#if defined(__x86_64__)

#define TSC_SUPPORTED true

static inline u64 rdtsc() {
    u32 lo, hi;
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi)); 
    return ((u64)hi << 32) | lo;
}

#elif defined(__i386__)

#define TSC_SUPPORTED true

static inline u64 rdtsc() {
    u64 result;
    asm volatile("rdtsc" : "=A" (result));
    return result;
}

#else

#define TSC_SUPPORTED false
#define rdtsc() 0

#endif


class TSC {
  private:
    static bool _initialized;
    static bool _enabled;
    static u64 _offset;
    static u64 _frequency;

  public:
    static void initialize();

    static bool initialized() {
        return TSC_SUPPORTED ? _initialized : true;
    }

    static bool enabled() {
        return TSC_SUPPORTED && _enabled;
    }

    static u64 ticks() {
        return enabled() ? rdtsc() - _offset : OS::nanotime();
    }

    static u64 frequency() {
        return _frequency;
    }
};

#endif // _TSC_H
