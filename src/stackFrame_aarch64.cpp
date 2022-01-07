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

#ifdef __aarch64__

#include <errno.h>
#include <sys/syscall.h>
#include "stackFrame.h"


#ifdef __APPLE__
#  define REG(l, m)  _ucontext->uc_mcontext->__ss.__##m
#else
#  define REG(l, m)  _ucontext->uc_mcontext.l
#endif


uintptr_t& StackFrame::pc() {
    return (uintptr_t&)REG(pc, pc);
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)REG(sp, sp);
}

uintptr_t& StackFrame::fp() {
    return (uintptr_t&)REG(regs[29], fp);
}

uintptr_t& StackFrame::retval() {
    return (uintptr_t&)REG(regs[0], x[0]);
}

uintptr_t StackFrame::arg0() {
    return (uintptr_t)REG(regs[0], x[0]);
}

uintptr_t StackFrame::arg1() {
    return (uintptr_t)REG(regs[1], x[1]);
}

uintptr_t StackFrame::arg2() {
    return (uintptr_t)REG(regs[2], x[2]);
}

uintptr_t StackFrame::arg3() {
    return (uintptr_t)REG(regs[3], x[3]);
}

void StackFrame::ret() {
    pc() = REG(regs[30], lr);
}


bool StackFrame::pop(bool trust_frame_pointer) {
    if (trust_frame_pointer && withinCurrentStack(fp())) {
        sp() = fp() + 16;
        fp() = stackAt(-2);
        pc() = stackAt(-1);
        return true;
    } else if (fp() == sp()) {
        fp() = stackAt(0);
        pc() = stackAt(1);
        sp() += 16;
        return true;
    }

    instruction_t insn = *(instruction_t*)pc();
    if ((insn & 0xffe07fff) == 0xa9007bfd) {
        // stp x29, x30, [sp, #offset]
        // SP has been adjusted, but FP not yet stored in a new frame
        unsigned int offset = (insn >> 12) & 0x1f8;
        sp() += offset + 16;
    }
    ret();
    return true;
}

bool StackFrame::checkInterruptedSyscall() {
#ifdef __APPLE__
    // We are not interested in syscalls that do not check error code, e.g. semaphore_wait_trap
    if (*(instruction_t*)pc() == 0xd65f03c0) {
        return true;
    }
    // If carry flag is set, the error code is in low byte of x0
    if (REG(pstate, cpsr) & (1 << 29)) {
        return (retval() & 0xff) == EINTR || (retval() & 0xff) == ETIMEDOUT;
    } else {
        return retval() == (uintptr_t)-EINTR; 
    }
#else
    return retval() == (uintptr_t)-EINTR;
#endif
}

int StackFrame::callerLookupSlots() {
    return 0;
}

bool StackFrame::isSyscall(instruction_t* pc) {
    // svc #0 or svc #80
    return (*pc & 0xffffefff) == 0xd4000001;
}

#endif // __aarch64__
