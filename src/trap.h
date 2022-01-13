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


const int TRAP_COUNT = 4;


class Trap {
  private:
    int _id;
    bool _unprotect;
    bool _protect;
    uintptr_t _entry;
    instruction_t _breakpoint_insn;
    instruction_t _saved_insn;

    bool patch(instruction_t insn);

    static uintptr_t _page_start[TRAP_COUNT];

  public:
    Trap(int id) : _id(id), _unprotect(true), _protect(WX_MEMORY), _entry(0), _breakpoint_insn(BREAKPOINT) {
    }

    uintptr_t entry() {
        return _entry;
    }

    bool covers(uintptr_t pc) {
        // PC points either to BREAKPOINT instruction or to the next one
        return pc - _entry <= sizeof(instruction_t);
    }

    void assign(const void* address, uintptr_t offset = BREAKPOINT_OFFSET);
    void pair(Trap& second);

    bool install() {
        return _entry == 0 || patch(_breakpoint_insn);
    }

    bool uninstall() {
        return _entry == 0 || patch(_saved_insn);
    }

    static bool isFaultInstruction(uintptr_t pc);
};

#endif // _TRAP_H
