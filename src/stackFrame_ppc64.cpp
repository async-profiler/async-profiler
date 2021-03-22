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
 *
 * Authors: Andrei Pangin and Gunter Haug
 */

#if defined(__PPC64__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#include <errno.h>
#include <sys/syscall.h>
#include "stackFrame.h"

uintptr_t& StackFrame::pc() {
    return (uintptr_t&)_ucontext->uc_mcontext.regs->nip;
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)_ucontext->uc_mcontext.regs->gpr[1];
}

uintptr_t& StackFrame::fp() {
    return *((uintptr_t*)_ucontext->uc_mcontext.regs->gpr[1]);
}

uintptr_t StackFrame::retval() {
    return (uintptr_t)_ucontext->uc_mcontext.regs->gpr[3];
}

uintptr_t StackFrame::arg0() {
    return (uintptr_t)_ucontext->uc_mcontext.regs->gpr[3];
}

uintptr_t StackFrame::arg1() {
    return (uintptr_t)_ucontext->uc_mcontext.regs->gpr[4];
}

uintptr_t StackFrame::arg2() {
    return (uintptr_t)_ucontext->uc_mcontext.regs->gpr[5];
}

uintptr_t StackFrame::arg3() {
    return (uintptr_t)_ucontext->uc_mcontext.regs->gpr[6];
}

void StackFrame::ret() {
    _ucontext->uc_mcontext.regs->nip = _ucontext->uc_mcontext.regs->link;
}

static inline bool inC1EpilogueCrit(uintptr_t pc) {
    if (!(pc & 0xFFF)) {
        // Make sure we are not at the page boundary, so that reading [pc - 1] is safe
        return false;
    }
    // C1 epilogue and critical section (posX)
    //        3821**** add     r1,r1,xx
    // pos3   xxxxxxxx
    // pos2   1000e1eb ld      r31,16(r1)
    // pos1   a603e87f mtlr    r31
    //        xxxxxxxx
    //        2000804e blr
    if (*((unsigned int *)pc + 1) == 0xebe10010 && *((unsigned int *)pc + 2) == 0x7fe803a6 ||
        *((unsigned int *)pc + 0) == 0xebe10010 && *((unsigned int *)pc + 1) == 0x7fe803a6 ||
        *((unsigned int *)pc - 1) == 0xebe10010 && *((unsigned int *)pc + 0) == 0x7fe803a6) {
        return true;
    }

    return false; // not in critical section
}

static inline bool inC2PrologueCrit(uintptr_t pc) {
    // C2 prologue and critical section
    //        f821**** stdu    r1, (xx)r1
    // pos1   fa950010 std     r20,16(r21)
    if (*((unsigned int *)pc) == 0xfa950010 && (*((unsigned int *)pc - 1) &0xFFFF0000 == 0xf8210000)) {
        return true;
    }

    return false; // not in critical section
}


bool StackFrame::pop(bool trust_frame_pointer) {
    // On PPC there is a valid back link to the previous frame at all times. The callee stores
    // the return address in the caller's frame before it constructs its own frame. After it
    // has destroyed its frame it restores the link register and returns. A problematic sequence
    // is the prologue/epilogue of a compiled method before/after frame construction/destruction.
    // Therefore popping the frame would not help here, as it is not yet/anymore present, rather
    // more adjusting the pc to the callers pc does the trick. There are two exceptions to this,
    // One in the prologue of C2 compiled methods and one in the epilogue of C1 compiled methods.
    if (inC1EpilogueCrit(pc())) {
        // lr not yet set: use the value stored in the frame
        pc() = stackAt(2);
    } else if (inC2PrologueCrit(pc())){
        // frame constructed but lr not yet stored in it: just do it here
        *(((unsigned long *) _ucontext->uc_mcontext.regs->gpr[21]) + 2) = (unsigned long) _ucontext->uc_mcontext.regs->gpr[20];
    } else {
        // most probably caller's framer is still on top but pc is already in callee: use caller's pc
        pc() = _ucontext->uc_mcontext.regs->link;
    }

    return true;
}



bool StackFrame::checkInterruptedSyscall() {
    return retval() == (uintptr_t)-EINTR;
}

int StackFrame::callerLookupSlots() {
    return 0;
}

bool StackFrame::isReturnAddress(instruction_t* pc) {
    return false;
}

bool StackFrame::isSyscall(instruction_t* pc) {
    // sc/svc
    return (*pc & 0x1F) == 17;
}

#endif // defined(__PPC64__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
