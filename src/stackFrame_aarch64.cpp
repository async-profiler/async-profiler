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
    return (uintptr_t)REG(regs[19], x[19]);
}

void StackFrame::ret() {
    pc() = link();
}

static inline bool isSTP(instruction_t insn) {
    // stp  xn, xm, [sp, #-imm]!
    // stp  dn, dm, [sp, #-imm]!
    return (insn & 0xffe003e0) == 0xa9a003e0 || (insn & 0xffe003e0) == 0x6da003e0;
}

// Check if this is a well-known leaf stub with a constant size frame
static inline bool isFixedSizeFrame(const char* name) {
    // Dispatch by the first character to optimize lookup
    switch (name[0]) {
        case 'i':
            return strncmp(name, "indexof_linear_", 15) == 0;
        case 'm':
            return strncmp(name, "md5_implCompress", 16) == 0;
        case 's':
            return strncmp(name, "sha256_implCompress", 19) == 0
                || strncmp(name, "string_indexof_linear_", 22) == 0
                || strncmp(name, "slow_subtype_check", 18) == 0;
        default:
            return false;
    }
}

// Check if this is a well-known leaf stub that does not change stack pointer
static inline bool isZeroSizeFrame(const char* name) {
    // Dispatch by the first character to optimize lookup
    switch (name[0]) {
        case 'I':
            return strcmp(name, "InlineCacheBuffer") == 0;
        case 'S':
            return strncmp(name, "SafeFetch", 9) == 0;
        case 'a':
            return strncmp(name, "atomic", 6) == 0;
        case 'b':
            return strncmp(name, "bigInteger", 10) == 0
                || strcmp(name, "base64_encodeBlock") == 0;
        case 'c':
            return strncmp(name, "copy_", 5) == 0
                || strncmp(name, "compare_long_string_", 20) == 0;
        case 'e':
            return strcmp(name, "encodeBlock") == 0;
        case 'f':
            return strcmp(name, "f2hf") == 0;
        case 'g':
            return strcmp(name, "ghash_processBlocks") == 0;
        case 'h':
            return strcmp(name, "hf2f") == 0;
        case 'i':
            return strncmp(name, "itable", 6) == 0;
        case 'l':
            return strcmp(name, "large_byte_array_inflate") == 0
                || strncmp(name, "lookup_secondary_supers_", 24) == 0;
        case 'm':
            return strncmp(name, "md5_implCompress", 16) == 0;
        case 's':
            return strncmp(name, "sha1_implCompress", 17) == 0
                || strncmp(name, "compare_long_string_same_encoding", 33) == 0
                || strcmp(name, "compare_long_string_LL") == 0
                || strcmp(name, "compare_long_string_UU") == 0;
        case 'u':
            return strcmp(name, "updateBytesAdler32") == 0;
        case 'v':
            return strncmp(name, "vtable", 6) == 0;
        case 'z':
            return strncmp(name, "zero_", 5) == 0;
        default:
            return false;
    }
}

bool StackFrame::unwindStub(instruction_t* entry, const char* name, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    if (ip == entry || *ip == 0xd65f03c0) {
        pc = link();
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
    } else if (entry != NULL && isSTP(entry[0]) && isFixedSizeFrame(name)) {
        // The stub begins with
        //   stp  xn, xm, [sp, #-imm]!
        int offset = int(entry[0] << 10) >> 25;
        sp = (intptr_t)sp - offset * 8;
        pc = link();
        return true;
    } else if (isZeroSizeFrame(name)) {
        // Should be done after isSTP check, since frame size may vary between JVM versions
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

static inline bool isFrameComplete(instruction_t* entry, instruction_t* ip) {
    // Frame is fully constructed after sp is decremented by the frame size.
    // Check if there is such an instruction anywhere between
    // the method entry and the current instruction pointer.
    while (--ip >= entry) {
        if ((*ip & 0xff8003ff) == 0xd10003ff) {  // sub sp, sp, #frame_size
            return true;
        }
    }
    return false;
}

bool StackFrame::unwindPrologue(NMethod* nm, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    // C1/C2 methods:
    //   {stack_bang}
    //   sub  sp, sp, #0x40
    //   stp  x29, x30, [sp, #48]
    //
    // Native wrappers:
    //   {stack_bang}
    //   stp  x29, x30, [sp, #-16]!
    //   mov  x29, sp
    //   sub  sp, sp, #0x50
    //
    instruction_t* ip = (instruction_t*)pc;
    instruction_t* entry = (instruction_t*)nm->entry();
    if (ip <= entry) {
        pc = link();
    } else if ((*ip & 0xffe07fff) == 0xa9007bfd) {
        // stp  x29, x30, [sp, #offset]
        // SP has been adjusted, but FP not yet stored in a new frame
        unsigned int offset = (*ip >> 12) & 0x1f8;
        sp += offset + 16;
        pc = link();
    } else if (ip[0] == 0x910003fd && ip[-1] == 0xa9bf7bfd) {
        // stp  x29, x30, [sp, #-16]!
        // mov  x29, sp
        sp += 16;
        pc = ((uintptr_t*)sp)[-1];
    } else if (ip <= entry + 16 && isFrameComplete(entry, ip)) {
        sp += nm->frameSize() * sizeof(void*);
        fp = ((uintptr_t*)sp)[-2];
        pc = ((uintptr_t*)sp)[-1];
    } else {
        pc = link();
    }
    return true;
}

static inline bool isPollReturn(instruction_t* ip) {
    // JDK 17+
    //   add  sp, sp, #0x30
    //   ldr  x8, [x28, #832]
    //   cmp  sp, x8
    //   b.hi offset
    //   ret
    //
    // JDK 11
    //   add  sp, sp, #0x30
    //   ldr  x8, [x28, #264]
    //   ldr  wzr, [x8]
    //   ret
    //
    // JDK 8
    //   add  sp, sp, #0x30
    //   adrp x8, polling_page
    //   ldr  wzr, [x8]
    //   ret
    //
    if ((ip[0] & 0xffc003ff) == 0xf9400388 && (ip[-1] & 0xff8003ff) == 0x910003ff) {
        // ldr x8, preceded by add sp
        return true;
    } else if ((ip[0] & 0x9f00001f) == 0x90000008 && (ip[-1] & 0xff8003ff) == 0x910003ff) {
        // adrp x8, preceded by add sp
        return true;
    } else if (ip[0] == 0xeb2863ff && ip[2] == 0xd65f03c0) {
        // cmp sp, x8, followed by ret
        return true;
    } else if ((ip[0] & 0xff000010) == 0x54000000 && ip[1] == 0xd65f03c0) {
        // b.cond, followed by ret
        return true;
    } else if (ip[0] == 0xb940011f && ip[1] == 0xd65f03c0) {
        // ldr wzr, followed by ret
        return true;
    }
    return false;
}

bool StackFrame::unwindEpilogue(NMethod* nm, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    //  ldp  x29, x30, [sp, #32]
    //  add  sp, sp, #0x30
    //  {poll_return}
    //  ret
    instruction_t* ip = (instruction_t*)pc;
    if (*ip == 0xd65f03c0 || isPollReturn(ip)) {  // ret
        pc = link();
        return true;
    }
    return false;
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
            if ((pc() & 0xfff) < max_distance && SafeAccess::load32((int32_t*)(pc() - max_distance)) == 0) {
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

bool StackFrame::isSyscall(instruction_t* pc) {
    // svc #0 or svc #80
    return (*pc & 0xffffefff) == 0xd4000001;
}

#endif // __aarch64__
