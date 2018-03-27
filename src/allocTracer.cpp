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

#include <unistd.h>
#include <sys/mman.h>
#include "allocTracer.h"
#include "profiler.h"
#include "stackFrame.h"
#include "vmStructs.h"


// JDK 7-9
Trap AllocTracer::_in_new_tlab("_ZN11AllocTracer33send_allocation_in_new_tlab_event");
Trap AllocTracer::_outside_tlab("_ZN11AllocTracer34send_allocation_outside_tlab_event");
// JDK 10+
Trap AllocTracer::_in_new_tlab2("_ZN11AllocTracer27send_allocation_in_new_tlab");
Trap AllocTracer::_outside_tlab2("_ZN11AllocTracer28send_allocation_outside_tlab");


// Resolve the address of the intercepted function
bool Trap::resolve(NativeCodeCache* libjvm) {
    if (_entry != NULL) {
        return true;
    }

    _entry = (instruction_t*)libjvm->findSymbol(_func_name);
    if (_entry != NULL) {
        // Make the entry point writable, so we can rewrite instructions
        long page_size = sysconf(_SC_PAGESIZE);
        uintptr_t page_start = (uintptr_t)_entry & -page_size;
        mprotect((void*)page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
        return true;
    }

    return false;
}

// Insert breakpoint at the very first instruction
void Trap::install() {
    if (_entry != NULL) {
        _saved_insn = *_entry;
        *_entry = BREAKPOINT;
        flushCache(_entry);
    }
}

// Clear breakpoint - restore the original instruction
void Trap::uninstall() {
    if (_entry != NULL) {
        *_entry = _saved_insn;
        flushCache(_entry);
    }
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
        recordAllocation(ucontext, frame.arg0(), frame.arg1(), false);
    } else if (frame.pc() - (uintptr_t)_outside_tlab._entry <= sizeof(instruction_t)) {
        // send_allocation_outside_tlab_event(KlassHandle klass, size_t alloc_size);
        recordAllocation(ucontext, frame.arg0(), frame.arg1(), true);
    } else if (frame.pc() - (uintptr_t)_in_new_tlab2._entry <= sizeof(instruction_t)) {
        // send_allocation_in_new_tlab(Klass* klass, HeapWord* obj, size_t tlab_size, size_t alloc_size, Thread* thread)
        recordAllocation(ucontext, frame.arg0(), frame.arg2(), false);
    } else if (frame.pc() - (uintptr_t)_outside_tlab2._entry <= sizeof(instruction_t)) {
        // send_allocation_outside_tlab(Klass* klass, HeapWord* obj, size_t alloc_size, Thread* thread)
        recordAllocation(ucontext, frame.arg0(), frame.arg2(), true);
    } else {
        // Not our trap; nothing to do
        return;
    }

    // Leave the trapped function by simulating "ret" instruction
    frame.ret();
}

void AllocTracer::recordAllocation(void* ucontext, uintptr_t rklass, uintptr_t rsize, bool outside_tlab) {
    VMSymbol* symbol = VMKlass::fromHandle(rklass)->name();
    if (outside_tlab) {
        // Invert the last bit to distinguish jmethodID from the allocation in new TLAB
        Profiler::_instance.recordSample(ucontext, rsize, BCI_SYMBOL_OUTSIDE_TLAB, (jmethodID)((uintptr_t)symbol ^ 1));
    } else {
        Profiler::_instance.recordSample(ucontext, rsize, BCI_SYMBOL, (jmethodID)symbol);
    }
}

Error AllocTracer::start(const char* event, long interval) {
    NativeCodeCache* libjvm = Profiler::_instance.jvmLibrary();
    if (libjvm == NULL) {
        return Error("libjvm not found among loaded libraries");
    }

    if (!VMStructs::init(libjvm)) {
        return Error("VMStructs unavailable. Unsupported JVM?");
    }

    if (!(_in_new_tlab.resolve(libjvm) || _in_new_tlab2.resolve(libjvm)) ||
        !(_outside_tlab.resolve(libjvm) || _outside_tlab2.resolve(libjvm))) {
        return Error("No AllocTracer symbols found. Are JDK debug symbols installed?");
    }

    installSignalHandler();

    _in_new_tlab.install();
    _outside_tlab.install();
    _in_new_tlab2.install();
    _outside_tlab2.install();

    return Error::OK;
}

void AllocTracer::stop() {
    _in_new_tlab.uninstall();
    _outside_tlab.uninstall();
    _in_new_tlab2.uninstall();
    _outside_tlab2.uninstall();
}
