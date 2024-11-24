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

class StackWalker {
  private:
    template<bool EXPERT_MODE>
    static int walkVM(void* ucontext, ASGCT_CallFrame* frames, int max_depth,
                      const void* pc, uintptr_t sp, uintptr_t fp);

  public:
    static int walkFP(void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx);
    static int walkDwarf(void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx);
    static int walkVM(void* ucontext, ASGCT_CallFrame* frames, int max_depth);
    static int walkVM(void* ucontext, ASGCT_CallFrame* frames, int max_depth, JavaFrameAnchor* anchor);

    static void checkFault();
};

#endif // _STACKWALKER_H
