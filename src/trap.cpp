/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/mman.h>
#include "trap.h"
#include "os.h"


uintptr_t Trap::_page_start[TRAP_COUNT] = {0};


bool Trap::isFaultInstruction(uintptr_t pc) {
    for (int i = 0; i < TRAP_COUNT; i++) {
        if (pc - _page_start[i] < OS::page_size) {
            return true;
        }
    }
    return false;
}

void Trap::assign(const void* address, uintptr_t offset) {
    _entry = (uintptr_t)address;
    if (_entry == 0) {
        return;
    }
    _entry += offset;

#if defined(__arm__) || defined(__thumb__)
    _breakpoint_insn = (_entry & 1) ? BREAKPOINT_THUMB : BREAKPOINT;
    _entry &= ~(uintptr_t)1;
#endif

    _saved_insn = *(instruction_t*)_entry;
    _page_start[_id] = _entry & -OS::page_size;
}

// Two allocation traps are always enabled/disabled together.
// If both traps belong to the same page, protect/unprotect it just once.
void Trap::pair(Trap& second) {
    if (_page_start[_id] == _page_start[second._id]) {
        _protect = false;
        second._unprotect = false;
    }
}

// Patch instruction at the entry point
bool Trap::patch(instruction_t insn) {
    if (_unprotect) {
        int prot = WX_MEMORY ? (PROT_READ | PROT_WRITE) : (PROT_READ | PROT_WRITE | PROT_EXEC);
        if (OS::mprotect((void*)(_entry & -OS::page_size), OS::page_size, prot) != 0) {
            return false;
        }
    }

    *(instruction_t*)_entry = insn;
    flushCache(_entry);

    if (_protect) {
        OS::mprotect((void*)(_entry & -OS::page_size), OS::page_size, PROT_READ | PROT_EXEC);
    }
    return true;
}
