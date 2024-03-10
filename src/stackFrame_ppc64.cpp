/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(__PPC64__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#include <errno.h>
#include <signal.h>
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

uintptr_t& StackFrame::retval() {
    return (uintptr_t&)_ucontext->uc_mcontext.regs->gpr[3];
}

uintptr_t StackFrame::link() {
    return (uintptr_t)_ucontext->uc_mcontext.regs->link;
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

uintptr_t StackFrame::jarg0() {
    // Unimplemented
    return 0;
}

uintptr_t StackFrame::method() {
    // Unimplemented
    return 0;
}

uintptr_t StackFrame::senderSP() {
    // Unimplemented
    return 0;
}

void StackFrame::ret() {
    pc() = link();
}

static inline bool inC1EpilogueCrit(uintptr_t pc) {
    if (!(pc & 0xfff)) {
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
    instruction_t* inst = (instruction_t*)pc;
    if (inst[ 1] == 0xebe10010 && inst[2] == 0x7fe803a6 ||
        inst[ 0] == 0xebe10010 && inst[1] == 0x7fe803a6 ||
        inst[-1] == 0xebe10010 && inst[0] == 0x7fe803a6) {
        return true;
    }

    return false; // not in critical section
}

static inline bool inC2PrologueCrit(uintptr_t pc) {
    // C2 prologue and critical section
    //        f821**** stdu    r1, (xx)r1
    // pos1   fa950010 std     r20,16(r21)
    instruction_t* inst = (instruction_t*)pc;
    if (inst[0] == 0xfa950010 && (inst[-1] & 0xffff0000) == 0xf8210000) {
        return true;
    }

    return false; // not in critical section
}


bool StackFrame::unwindStub(instruction_t* entry, const char* name, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    pc = link();
    return true;
}

bool StackFrame::unwindCompiled(NMethod* nm, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    // On PPC there is a valid back link to the previous frame at all times. The callee stores
    // the return address in the caller's frame before it constructs its own frame. After it
    // has destroyed its frame it restores the link register and returns. A problematic sequence
    // is the prologue/epilogue of a compiled method before/after frame construction/destruction.
    // Therefore popping the frame would not help here, as it is not yet/anymore present, rather
    // more adjusting the pc to the callers pc does the trick. There are two exceptions to this,
    // One in the prologue of C2 compiled methods and one in the epilogue of C1 compiled methods.
    if (inC1EpilogueCrit(pc)) {
        // lr not yet set: use the value stored in the frame
        pc = ((uintptr_t*)sp)[2];
    } else if (inC2PrologueCrit(pc)) {
        // frame constructed but lr not yet stored in it: just do it here
        *(((unsigned long *) _ucontext->uc_mcontext.regs->gpr[21]) + 2) = (unsigned long) _ucontext->uc_mcontext.regs->gpr[20];
    } else {
        // most probably caller's framer is still on top but pc is already in callee: use caller's pc
        pc = link();
    }

    return true;
}

bool StackFrame::unwindAtomicStub(const void*& pc) {
    // Not needed
    return false;
}

void StackFrame::adjustSP(const void* entry, const void* pc, uintptr_t& sp) {
    // Not needed
}

bool StackFrame::skipFaultInstruction() {
    return false;
}

bool StackFrame::checkInterruptedSyscall() {
    return retval() == (uintptr_t)-EINTR;
}

bool StackFrame::isSyscall(instruction_t* pc) {
    // sc/svc
    return (*pc & 0x1f) == 17;
}

#endif // defined(__PPC64__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
