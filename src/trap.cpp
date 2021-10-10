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

#include <sys/mman.h>
#include "trap.h"
#include "os.h"
#include "stackFrame.h"


uintptr_t Trap::_page_start[TRAP_COUNT] = {0};
struct sigaction Trap::_jvm_handler = {NULL};


void Trap::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    // Ignore access violations on our traps
    uintptr_t pc = StackFrame(ucontext).pc();
    for (int i = 0; i < TRAP_COUNT; i++) {
        if (pc - _page_start[i] < OS::page_size) {
            return;
        }
    }

    // Otherwise, delegate to the JVM signal handler
    _jvm_handler.sa_sigaction(signo, siginfo, ucontext);
}

void Trap::assign(const void* address, uintptr_t offset) {
    _entry = (uintptr_t)address;
    if (_entry == 0) {
        return;
    }
    _entry += offset;

#if defined(__arm__) || defined(__thumb__)
    _breakpoint_insn = (_entry & 1) ? BREAKPOINT_THUMB : BREAKPOINT;
    _entry ^= 1;
#endif

    _saved_insn = *(instruction_t*)_entry;
    _page_start[_id] = _entry & -OS::page_size;

    if (WX_MEMORY && _jvm_handler.sa_sigaction == NULL) {
        // While we are patching the code in RW mode, another thread may attempt to execute it
        sigaction(SIGBUS, NULL, &_jvm_handler);
        struct sigaction sa = _jvm_handler;
        sa.sa_sigaction = signalHandler;
        sigaction(SIGBUS, &sa, NULL);
    }
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
        if (mprotect((void*)(_entry & -OS::page_size), OS::page_size, prot) != 0) {
            return false;
        }
    }

    *(instruction_t*)_entry = insn;
    flushCache(_entry);

    if (_protect) {
        mprotect((void*)(_entry & -OS::page_size), OS::page_size, PROT_READ | PROT_EXEC);
    }
    return true;
}
