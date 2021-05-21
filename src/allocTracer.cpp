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

#include "allocTracer.h"
#include "profiler.h"
#include "stackFrame.h"
#include "vmStructs.h"


int AllocTracer::_trap_kind;
Trap AllocTracer::_in_new_tlab;
Trap AllocTracer::_outside_tlab;

u64 AllocTracer::_interval;
volatile u64 AllocTracer::_allocated_bytes;


// Called whenever our breakpoint trap is hit
void AllocTracer::trapHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    StackFrame frame(ucontext);
    int event_type;
    uintptr_t total_size;
    uintptr_t instance_size;

    // PC points either to BREAKPOINT instruction or to the next one
    if (_in_new_tlab.covers(frame.pc())) {
        // send_allocation_in_new_tlab(Klass* klass, HeapWord* obj, size_t tlab_size, size_t alloc_size, Thread* thread)
        // send_allocation_in_new_tlab_event(KlassHandle klass, size_t tlab_size, size_t alloc_size)
        event_type = BCI_ALLOC;
        total_size = _trap_kind == 1 ? frame.arg2() : frame.arg1();
        instance_size = _trap_kind == 1 ? frame.arg3() : frame.arg2();
    } else if (_outside_tlab.covers(frame.pc())) {
        // send_allocation_outside_tlab(Klass* klass, HeapWord* obj, size_t alloc_size, Thread* thread)
        // send_allocation_outside_tlab_event(KlassHandle klass, size_t alloc_size);
        event_type = BCI_ALLOC_OUTSIDE_TLAB;
        total_size = _trap_kind == 1 ? frame.arg2() : frame.arg1();
        instance_size = 0;
    } else {
        // Not our trap
        Profiler::_instance.trapHandler(signo, siginfo, ucontext);
        return;
    }

    // Leave the trapped function by simulating "ret" instruction
    uintptr_t klass = frame.arg0();
    frame.ret();

    if (_enabled) {
        recordAllocation(ucontext, event_type, klass, total_size, instance_size);
    }
}

void AllocTracer::recordAllocation(void* ucontext, int event_type, uintptr_t rklass,
                                   uintptr_t total_size, uintptr_t instance_size) {
    if (_interval > 1) {
        // Do not record allocation unless allocated at least _interval bytes
        while (true) {
            u64 prev = _allocated_bytes;
            u64 next = prev + total_size;
            if (next < _interval) {
                if (__sync_bool_compare_and_swap(&_allocated_bytes, prev, next)) {
                    return;
                }
            } else {
                if (__sync_bool_compare_and_swap(&_allocated_bytes, prev, next % _interval)) {
                    break;
                }
            }
        }
    }

    AllocEvent event;
    event._class_id = 0;
    event._total_size = total_size;
    event._instance_size = instance_size;

    if (VMStructs::hasClassNames()) {
        VMSymbol* symbol = VMKlass::fromHandle(rklass)->name();
        event._class_id = Profiler::_instance.classMap()->lookup(symbol->body(), symbol->length());
    }

    Profiler::_instance.recordSample(ucontext, total_size, event_type, &event);
}

Error AllocTracer::check(Arguments& args) {
    if (_in_new_tlab.entry() != 0 && _outside_tlab.entry() != 0) {
        return Error::OK;
    }

    NativeCodeCache* libjvm = VMStructs::libjvm();
    const void* ne;
    const void* oe;

    if ((ne = libjvm->findSymbolByPrefix("_ZN11AllocTracer27send_allocation_in_new_tlab")) != NULL &&
        (oe = libjvm->findSymbolByPrefix("_ZN11AllocTracer28send_allocation_outside_tlab")) != NULL) {
        _trap_kind = 1;  // JDK 10+
    } else if ((ne = libjvm->findSymbolByPrefix("_ZN11AllocTracer33send_allocation_in_new_tlab_eventE11KlassHandleP8HeapWord")) != NULL &&
               (oe = libjvm->findSymbolByPrefix("_ZN11AllocTracer34send_allocation_outside_tlab_eventE11KlassHandleP8HeapWord")) != NULL) {
        _trap_kind = 1;  // JDK 8u262+
    } else if ((ne = libjvm->findSymbolByPrefix("_ZN11AllocTracer33send_allocation_in_new_tlab_event")) != NULL &&
               (oe = libjvm->findSymbolByPrefix("_ZN11AllocTracer34send_allocation_outside_tlab_event")) != NULL) {
        _trap_kind = 2;  // JDK 7-9
    } else {
        return Error("No AllocTracer symbols found. Are JDK debug symbols installed?");
    }

    if (!_in_new_tlab.assign(offsetBPaddr(ne)) || !_outside_tlab.assign(offsetBPaddr(oe))) {
        return Error("Unable to install allocation trap");
    }


    return Error::OK;
}

Error AllocTracer::start(Arguments& args) {
    Error error = check(args);
    if (error) {
        return error;
    }

    _interval = args._alloc;
    _allocated_bytes = 0;

    _in_new_tlab.install();
    _outside_tlab.install();

    return Error::OK;
}

void AllocTracer::stop() {
    _in_new_tlab.uninstall();
    _outside_tlab.uninstall();
}
