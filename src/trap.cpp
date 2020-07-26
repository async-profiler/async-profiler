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

#include <unistd.h>
#include <sys/mman.h>
#include "trap.h"


bool Trap::assign(const void* address) {
    uintptr_t entry = (uintptr_t)address;
    if (entry == 0) {
        _entry = 0;
        return true;
    }

#if defined(__arm__) || defined(__thumb__)
    if (entry & 1) {
        entry ^= 1;
        _breakpoint_insn = BREAKPOINT_THUMB;
    }
#endif

    if (entry != _entry) {
        // Make the entry point writable, so we can rewrite instructions
        long page_size = sysconf(_SC_PAGESIZE);
        if (mprotect((void*)(entry & -page_size), page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
            return false;
        }
        _entry = entry;
        _saved_insn = *(instruction_t*)entry;
    }

    return true;
}

// Insert breakpoint at the very first instruction
void Trap::install() {
    if (_entry) {
        *(instruction_t*)_entry = _breakpoint_insn;
        flushCache(_entry);
    }
}

// Clear breakpoint - restore the original instruction
void Trap::uninstall() {
    if (_entry) {
        *(instruction_t*)_entry = _saved_insn;
        flushCache(_entry);
    }
}
