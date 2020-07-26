/*
 * Copyright 2020 Andrei Pangin
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

#ifndef _TRAP_H
#define _TRAP_H

#include <stdint.h>
#include "arch.h"


class Trap {
  private:
    uintptr_t _entry;
    instruction_t _breakpoint_insn;
    instruction_t _saved_insn;

  public:
    Trap() : _entry(0), _breakpoint_insn(BREAKPOINT) {
    }

    uintptr_t entry() {
        return _entry;
    }

    bool covers(uintptr_t pc) {
        // PC points either to BREAKPOINT instruction or to the next one
        return pc - _entry <= sizeof(instruction_t);
    }

    bool assign(const void* address);
    void install();
    void uninstall();
};

#endif // _TRAP_H
