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


class SafeAccess {
  public:
    __attribute__((noinline,aligned(16)))
    static uintptr_t load(uintptr_t ptr) {
        return *(uintptr_t*)ptr;
    }

    static bool isFaultInstruction(uintptr_t pc) {
        return pc - (uintptr_t)load < 16;
    }
};

#endif // _SAFEACCESS_H
