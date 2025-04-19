/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __aarch64__

#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include "stackFrame.h"
#include "safeAccess.h"
#include "vmStructs.h"
#include "vmEntry.h"


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

uintptr_t StackFrame::link() {
    return (uintptr_t)REG(regs[30], lr);
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

uintptr_t StackFrame::jarg0() {
    return arg1();
}

uintptr_t StackFrame::method() {
    return (uintptr_t)REG(regs[12], x[12]);
}

uintptr_t StackFrame::senderSP() {
    // On JDK 8/11/17, sender sp is stored in x13, and on JDK 20 and above, sender sp is stored in x19
    // https://bugs.openjdk.org/browse/JDK-8288971
    // https://github.com/openjdk/jdk8u-dev/blob/55273f7267b95cf38743bb32ea61a513fbafb06e/hotspot/src/cpu/aarch64/vm/templateInterpreter_aarch64.cpp#L633
    // https://github.com/openjdk/jdk17u-dev/blob/85d0ab55d6bae2aea4368e6668e361db8a9cbd1c/src/hotspot/cpu/aarch64/templateInterpreterGenerator_aarch64.cpp#L841
    // https://github.com/openjdk/jdk21u-dev/blob/4daaffcd9dae87b5b51f9277e7f407a7d31a1eb9/src/hotspot/cpu/aarch64/templateInterpreterGenerator_aarch64.cpp#L870
    if (VM::hotspot_version() >= 20) {
        return (uintptr_t)REG(regs[19], x[19]);
    } else {
        return (uintptr_t)REG(regs[13], x[13]);
    }
}

void StackFrame::ret() {
    pc() = link();
}


bool StackFrame::unwindStub(instruction_t* entry, const char* name, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    if (ip == entry || *ip == 0xd65f03c0
        || strncmp(name, "itable", 6) == 0
        || strncmp(name, "vtable", 6) == 0
        || strncmp(name, "compare_long_string_", 20) == 0
        || strcmp(name, "zero_blocks") == 0
        || strcmp(name, "atomic entry points") == 0
        || strcmp(name, "InlineCacheBuffer") == 0)
    {
        pc = link();
        return true;
    } else if (strcmp(name, "forward_copy_longs") == 0
            || strcmp(name, "backward_copy_longs") == 0
            // There is a typo in JDK 8
            || strcmp(name, "foward_copy_longs") == 0) {
        // These are called from arraycopy stub that maintains the regular frame link
        if (&pc == &this->pc() && withinCurrentStack(fp)) {
            // Unwind both stub frames for AsyncGetCallTrace
            sp = fp + 16;
            fp = ((uintptr_t*)sp)[-2];
            pc = ((uintptr_t*)sp)[-1] - sizeof(instruction_t);
        } else {
            // When cstack=vm, unwind stub frames one by one
            pc = link();
        }
        return true;
    } else if (entry != NULL && entry[0] == 0xa9bf7bfd) {
        // The stub begins with
        //   stp  x29, x30, [sp, #-16]!
        //   mov  x29, sp
        if (ip == entry + 1) {
            sp += 16;
            pc = ((uintptr_t*)sp)[-1];
            return true;
        } else if (entry[1] == 0x910003fd && withinCurrentStack(fp)) {
            sp = fp + 16;
            fp = ((uintptr_t*)sp)[-2];
            pc = ((uintptr_t*)sp)[-1];
            return true;
        }
    } else if (strncmp(name, "indexof_linear_", 15) == 0 &&
               entry != NULL && entry[0] == 0xa9be57f4 && entry[1] == 0xa9015ff6) {
        // JDK-8189103: String.indexOf intrinsic.
        // Entry and exit are covered by the very first 'if', in all other cases SP is 4 words off.
        sp += 32;
        pc = link();
        return true;
    }
    return false;
}

static inline bool isEntryBarrier(instruction_t* ip) {
    // ldr  w9, [x28, #32]
    // cmp  x8, x9
    return ip[0] == 0xb9402389 && ip[1] == 0xeb09011f;
}

bool StackFrame::unwindCompiled(NMethod* nm, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    instruction_t* entry = (instruction_t*)nm->entry();
    if ((*ip & 0xffe07fff) == 0xa9007bfd) {
        // stp  x29, x30, [sp, #offset]
        // SP has been adjusted, but FP not yet stored in a new frame
        unsigned int offset = (*ip >> 12) & 0x1f8;
        sp += offset + 16;
        pc = link();
    } else if (ip > entry && ip[0] == 0x910003fd && ip[-1] == 0xa9bf7bfd) {
        // stp  x29, x30, [sp, #-16]!
        // mov  x29, sp
        sp += 16;
        pc = ((uintptr_t*)sp)[-1];
    } else if (ip > entry + 3 && !nm->isFrameCompleteAt(ip) &&
               (isEntryBarrier(ip) || isEntryBarrier(ip + 1))) {
        // Frame should be complete at this point
        sp += nm->frameSize() * sizeof(void*);
        fp = ((uintptr_t*)sp)[-2];
        pc = ((uintptr_t*)sp)[-1];
    } else {
        // Just try
        pc = link();
    }
    return true;
}

bool StackFrame::unwindAtomicStub(const void*& pc) {
    // VM threads may call generated atomic stubs, which are not normally walkable
    const void* lr = (const void*)link();
    if (VMStructs::libjvm()->contains(lr)) {
        NMethod* nm = CodeHeap::findNMethod(pc);
        if (nm != NULL && strncmp(nm->name(), "Stub", 4) == 0) {
            pc = lr;
            return true;
        }
    }
    return false;
}

void StackFrame::adjustSP(const void* entry, const void* pc, uintptr_t& sp) {
    instruction_t* ip = (instruction_t*)pc;
    if (ip > entry && (ip[-1] == 0xa9bf27ff || (ip[-1] == 0xd63f0100 && ip[-2] == 0xa9bf27ff))) {
        // When calling a leaf native from Java, JVM puts a dummy frame link onto the stack,
        // thus breaking the invariant: sender_sp == current_sp + frame_size.
        // Since JDK 21, there are more instructions between `blr` and `add`,
        // ignore them now for the sake of simplicity.
        //   stp  xzr, x9, [sp, #-16]!
        //   blr  x8
        //   ...
        //   add  sp, sp, #0x10
        sp += 16;
    }
}

bool StackFrame::skipFaultInstruction() {
    return false;
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
    if (retval() == (uintptr_t)-EINTR) {
        // Workaround for JDK-8237858: restart the interrupted poll / epoll_wait manually
        uintptr_t nr = (uintptr_t)REG(regs[8], x[8]);
        if (nr == SYS_ppoll || (nr == SYS_epoll_pwait && (int)arg3() == -1)) {
            // Check against unreadable page for the loop below
            const uintptr_t max_distance = 24;
            if ((pc() & 0xfff) < max_distance && SafeAccess::load32((u32*)(pc() - max_distance), 0) == 0) {
                return true;
            }
            // Try to restore the original value of x0 saved in another register
            for (uintptr_t prev_pc = pc() - 4; pc() - prev_pc <= max_distance; prev_pc -= 4) {
                instruction_t insn = *(instruction_t*)prev_pc;
                unsigned int reg = (insn >> 16) & 31;
                if ((insn & 0xffe0ffff) == 0xaa0003e0 && reg >= 6) {
                    // mov x0, reg
                    REG(regs[0], x[0]) = REG(regs[reg], x[reg]);
                    pc() -= sizeof(instruction_t);
                    break;
                }
            }
        }
        return true;
    }
    return false;
#endif
}

void StackFrame::unwindIncompleteFrame(uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    pc = (uintptr_t)stripPointer(*(void**)sp);
    sp = senderSP();
}

bool StackFrame::isSyscall(instruction_t* pc) {
    // svc #0 or svc #80
    return (*pc & 0xffffefff) == 0xd4000001;
}

// On aarch64, for [method entry point], is_plausible_interpreter_frame is always true,
// so we need to parse the instructions to determine whether sender sp is on the stack.
// If the instruction is in the range of (set fp = sp, push sender sp], then sender sp is not on the stack.
bool StackFrame::isSenderSPOnStack(instruction_t* pc, bool is_plausible_interpreter_frame) {
    // Bellow OpenJDK 20
    // 0x0000ffff873184e4:   stp	x29, x30, [sp, #80]     <- push fp, lr    // 0xa9057bfd
    // 0x0000ffff873184e8:   add	x29, sp, #0x50          <- set fp = sp    // 0x910143fd
    // 0x0000ffff873184ec:   stp	xzr, x13, [sp, #64]     <- push sender sp // 0xa90437ff

    // OpenJDK 20 and above
    // 0x0000ffffac4e76f4:   stp	x20, x22, [sp, #-96]! <- push java expression stack pointer, bcp
    // 0x0000ffffac4e76f8:   stp	xzr, x12, [sp, #48]   <- push Method*
    // 0x0000ffffac4e76fc:   stp	x29, x30, [sp, #80]   <- push fp, lr  // 0xa9057bfd
    // 0x0000ffffac4e7700:   add	x29, sp, #0x50        <- set fp = sp  // 0x910143fd
    // 0x0000ffffac4e7704:   ldr	x26, [x12, #8]                        // 0xf940059a
    // 0x0000ffffac4e7708:   ldr	x26, [x26, #8]                        // 0xf940075a
    // 0x0000ffffac4e770c:   ldr	x26, [x26, #16]                       // 0xf9400b5a
    // 0x0000ffffac4e7710:   sub	x8, x24, x29                          // 0xcb1d0308
    // 0x0000ffffac4e7714:   lsr	x8, x8, #3                            // 0xd343fd08
    // 0x0000ffffac4e7718:   stp	x8, x26, [sp, #16]                    // 0xa9016be8
    // 0x0000ffffac4e771c:   stp	xzr, x19, [sp, #64] <- push sender sp // 0xa9044fff

    instruction_t v = *pc;
    return !(
        (v == 0xf940059a && *(pc - 1) == 0x910143fd) || // check the current and the [set fp = sp] instruction
        (v == 0xf940075a && *(pc - 2) == 0x910143fd) ||
        (v == 0xf9400b5a && *(pc - 3) == 0x910143fd) ||
        (v == 0xcb1d0308 && *(pc - 4) == 0x910143fd) ||
        (v == 0xd343fd08 && *(pc - 5) == 0x910143fd) ||
        (v == 0xa9016be8 && *(pc - 6) == 0x910143fd) ||
        (v == 0xa9044fff && *(pc - 7) == 0x910143fd) ||
        (v == 0xa90437ff && *(pc - 1) == 0x910143fd)
        );
}
#endif // __aarch64__
