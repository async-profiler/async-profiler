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

#include <stdint.h>
#include <sys/mman.h>
#include "allocTracer.h"
#include "codeCache.h"
#include "profiler.h"
#include "stackFrame.h"
#include "vmStructs.h"


Trap AllocTracer::_in_new_tlab("_ZN11AllocTracer33send_allocation_in_new_tlab_event");
Trap AllocTracer::_outside_tlab("_ZN11AllocTracer34send_allocation_outside_tlab_event");


// Make the entry point writeable and insert breakpoint at the very first instruction
void Trap::install() {
    uintptr_t page_start = (uintptr_t)_entry & ~PAGE_MASK;
    mprotect((void*)page_start, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC);

    _saved_insn = *_entry;
    *_entry = BREAKPOINT;
    flushCache(_entry);
}

// Clear breakpoint - restore the original instruction
void Trap::uninstall() {
    *_entry = _saved_insn;
    flushCache(_entry);
}


void AllocTracer::installSignalHandler() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = NULL;
    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;

    sigaction(SIGTRAP, &sa, NULL);
}

// Called whenever our breakpoint trap is hit
void AllocTracer::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    StackFrame frame(ucontext);

    // PC points either to BREAKPOINT instruction or to the next one
    if (frame.pc() - (uintptr_t)_in_new_tlab._entry <= sizeof(instruction_t)) {
        // send_allocation_in_new_tlab_event(KlassHandle klass, size_t tlab_size, size_t alloc_size)
        jmethodID alloc_class = (jmethodID)frame.arg0();
        u64 obj_size = frame.arg2();
        Profiler::_instance.recordSample(ucontext, obj_size, BCI_KLASS, alloc_class);
    } else if (frame.pc() - (uintptr_t)_outside_tlab._entry <= sizeof(instruction_t)) {
        // send_allocation_outside_tlab_event(KlassHandle klass, size_t alloc_size);
        // Invert last bit to distinguish jmethodID from the allocation in new TLAB
        jmethodID alloc_class = (jmethodID)(frame.arg0() ^ 1);
        u64 obj_size = frame.arg1();
        Profiler::_instance.recordSample(ucontext, obj_size, BCI_KLASS_OUTSIDE_TLAB, alloc_class);
    } else {
        // Not our trap; nothing to do
        return;
    }

    // Leave the trapped function by simulating "ret" instruction
    frame.ret();
}

Error AllocTracer::start() {
    NativeCodeCache* libjvm = Profiler::_instance.jvmLibrary();
    if (libjvm == NULL) {
        return Error("libjvm not found among loaded libraries");
    }

    if (!VMStructs::init(libjvm)) {
        return Error("VMStructs unavailable. Unsupported JVM?");
    }

    if (_in_new_tlab._entry == NULL || _outside_tlab._entry == NULL) {
        _in_new_tlab._entry = (instruction_t*)libjvm->findSymbol(_in_new_tlab._func_name);
        _outside_tlab._entry = (instruction_t*)libjvm->findSymbol(_outside_tlab._func_name);
        if (_in_new_tlab._entry == NULL || _outside_tlab._entry == NULL) {
            return Error("No AllocTracer symbols found. Are JDK debug symbols installed?");
        }
    }

    installSignalHandler();

    _in_new_tlab.install();
    _outside_tlab.install();

    return Error::OK;
}

void AllocTracer::stop() {
    _in_new_tlab.uninstall();
    _outside_tlab.uninstall();
}
