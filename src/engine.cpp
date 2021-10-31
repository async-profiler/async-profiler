/*
 * Copyright 2018 Andrei Pangin
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

#include "engine.h"
#include "stackFrame.h"
#include "vmStructs.h"


volatile bool Engine::_enabled;

Error Engine::check(Arguments& args) {
    return Error::OK;
}

Error Engine::start(Arguments& args) {
    return Error::OK;
}

void Engine::stop() {
}

int Engine::getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth) {
    const void* pc;
    uintptr_t fp;
    uintptr_t prev_fp = (uintptr_t)&fp;
    uintptr_t bottom = prev_fp + 0x100000;

    if (ucontext == NULL) {
        pc = __builtin_return_address(0);
        fp = (uintptr_t)__builtin_frame_address(1);
    } else {
        StackFrame frame(ucontext);
        pc = (const void*)frame.pc();
        fp = frame.fp();
    }

    int depth = 0;
    const void* const valid_pc = (const void* const)0x1000;

    // Walk until the bottom of the stack or until the first Java frame
    while (depth < max_depth && pc >= valid_pc) {
        if (CodeHeap::contains(pc)) {
            break;
        }

        callchain[depth++] = pc;

        // Check if the next frame is below on the current stack
        if (fp <= prev_fp || fp >= prev_fp + 0x40000 || fp >= bottom) {
            break;
        }

        // Frame pointer must be word aligned
        if ((fp & (sizeof(uintptr_t) - 1)) != 0) {
            break;
        }

        prev_fp = fp;
        pc = stripPointer(((const void**)fp)[FRAME_PC_SLOT]);
        fp = ((uintptr_t*)fp)[0];
    }

    return depth;
}
