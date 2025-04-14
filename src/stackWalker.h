/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _STACKWALKER_H
#define _STACKWALKER_H

#include <stdint.h>
#include "vmEntry.h"


class JavaFrameAnchor;

struct StackContext {
    const void* pc;
    uintptr_t sp;
    uintptr_t fp;

    void set(const void* pc, uintptr_t sp, uintptr_t fp) {
        this->pc = pc;
        this->sp = sp;
        this->fp = fp;
    }
};

// Detail level of VMStructs stack walking
enum StackDetail {
    VM_BASIC,   // only basic Java frames similar to what AsyncGetCallTrace provides
    VM_NORMAL,  // include frame types and runtime stubs
    VM_EXPERT   // all features: frame types, runtime stubs, and intermediate native frames
};

class StackWalker {
  private:
    static int walkVM(void* ucontext, ASGCT_CallFrame* frames, int max_depth,
                      StackDetail detail, const void* pc, uintptr_t sp, uintptr_t fp, bool* truncated);

  public:
    static int walkFP(void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx, bool* truncated);
    static int walkDwarf(void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx, bool* truncated);
    static int walkVM(void* ucontext, ASGCT_CallFrame* frames, int max_depth, StackDetail detail, bool* truncated);
    static int walkVM(void* ucontext, ASGCT_CallFrame* frames, int max_depth, JavaFrameAnchor* anchor, bool* truncated);

    static void checkFault();
};

#endif // _STACKWALKER_H
