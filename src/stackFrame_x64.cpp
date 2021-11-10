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

#ifdef __x86_64__

#include <errno.h>
#include <sys/syscall.h>
#include "stackFrame.h"


#ifdef __APPLE__
#  define REG(l, m)  _ucontext->uc_mcontext->__ss.__##m
#else
#  define REG(l, m)  _ucontext->uc_mcontext.gregs[REG_##l]
#endif


uintptr_t& StackFrame::pc() {
    return (uintptr_t&)REG(RIP, rip);
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)REG(RSP, rsp);
}

uintptr_t& StackFrame::fp() {
    return (uintptr_t&)REG(RBP, rbp);
}

uintptr_t StackFrame::retval() {
    return (uintptr_t)REG(RAX, rax);
}

uintptr_t StackFrame::arg0() {
    return (uintptr_t)REG(RDI, rdi);
}

uintptr_t StackFrame::arg1() {
    return (uintptr_t)REG(RSI, rsi);
}

uintptr_t StackFrame::arg2() {
    return (uintptr_t)REG(RDX, rdx);
}

uintptr_t StackFrame::arg3() {
    return (uintptr_t)REG(RCX, rcx);
}

void StackFrame::ret() {
    pc() = stackAt(0);
    sp() += 8;
}


static inline bool isFramePrologueEpilogue(uintptr_t pc) {
    if (pc & 0xfff) {
        // Make sure we are not at the page boundary, so that reading [pc - 1] is safe
        unsigned int opcode = *(unsigned int*)(pc - 1);
        if (opcode == 0xec834855) {
            // push rbp
            // sub  rsp, $const
            return true;
        } else if (opcode == 0xec8b4855) {
            // push rbp
            // mov  rbp, rsp
            return true;
        }
    }

    if (*(unsigned char*)pc == 0x5d && *(unsigned short*)(pc + 1) == 0x0585) {
        // pop  rbp
        // test [polling_page], eax
        return true;
    }

    return false;
}

bool StackFrame::pop(bool trust_frame_pointer) {
    if (trust_frame_pointer && withinCurrentStack(fp())) {
        sp() = fp() + 16;
        fp() = stackAt(-2);
        pc() = stackAt(-1);
        return true;
    } else if (fp() == sp() || withinCurrentStack(stackAt(0)) || isFramePrologueEpilogue(pc())) {
        fp() = stackAt(0);
        pc() = stackAt(1);
        sp() += 16;
        return true;
    }
    return false;
}

bool StackFrame::checkInterruptedSyscall() {
#ifdef __APPLE__
    // We are not interested in syscalls that do not check error code, e.g. semaphore_wait_trap
    if (*(instruction_t*)pc() == 0xc3) {
        return true;
    }
    // If CF is set, the error code is in low byte of eax,
    // some other syscalls (ulock_wait) do not set CF when interrupted
    if (REG(EFL, rflags) & 1) {
        return (retval() & 0xff) == EINTR || (retval() & 0xff) == ETIMEDOUT;
    } else {
        return retval() == (uintptr_t)-EINTR; 
    }
#else
    if (retval() == (uintptr_t)-EINTR) {
        // Workaround for JDK-8237858: restart the interrupted poll() manually.
        // Check if the previous instruction is mov eax, SYS_poll with infinite timeout
        if ((int)arg2() == -1) {
            uintptr_t pc = this->pc();
            if ((pc & 0xfff) >= 7 && *(unsigned char*)(pc - 7) == 0xb8 && *(int*)(pc - 6) == SYS_poll) {
                this->pc() = pc - 7;
            }
        }
        return true;
    }
    return false;
#endif
}

int StackFrame::callerLookupSlots() {
    return 7;
}

bool StackFrame::isSyscall(instruction_t* pc) {
    return pc[0] == 0x0f && pc[1] == 0x05;
}

#endif // __x86_64__
