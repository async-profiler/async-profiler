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

#include "safeAccess.h"


__attribute__((noinline,aligned(16)))
uintptr_t SafeAccess::load(uintptr_t ptr) {
    register uintptr_t result;
    asm volatile("movq (%1), %0" : "=a"(result) : "r"(ptr));
    return result;
}

bool SafeAccess::checkProtection(uintptr_t& pc) {
    if (pc - (uintptr_t)load < 16) {
        pc += 3;
        return true;
    }
    return false;
}
