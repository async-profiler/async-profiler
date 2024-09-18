/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <dlfcn.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "profiler.h"
#include "perfEvents.h"
#include "ctimer.h"
#include "allocTracer.h"
#include "lockTracer.h"
#include "wallClock.h"
#include "j9ObjectSampler.h"
#include "j9StackTraces.h"
#include "j9WallClock.h"
#include "instrument.h"
#include "itimer.h"
#include "dwarf.h"
#include "flameGraph.h"
#include "flightRecorder.h"
#include "fdtransferClient.h"
#include "frameName.h"
#include "os.h"
#include "safeAccess.h"
#include "stackFrame.h"
#include "stackWalker.h"
#include "symbols.h"
#include "tsc.h"
#include "vmStructs.h"


// The instance is not deleted on purpose, since profiler structures
// can be still accessed concurrently during VM termination
Profiler* const Profiler::_instance = new Profiler();

static SigAction orig_trapHandler = NULL;
static SigAction orig_segvHandler = NULL;

static Engine noop_engine;
static PerfEvents perf_events;
static AllocTracer alloc_tracer;
static LockTracer lock_tracer;
static ObjectSampler object_sampler;
static J9ObjectSampler j9_object_sampler;
static WallClock wall_clock;
static J9WallClock j9_wall_clock;
static CTimer ctimer;
static ITimer itimer;
static Instrument instrument;

static ProfilingWindow profiling_window;


// The same constants are used in JfrSync
enum EventMask {
    EM_CPU   = 1,
    EM_ALLOC = 2,
    EM_LOCK  = 4,
    EM_WALL  = 8
};


struct MethodSample {
    u64 samples;
    u64 counter;

    void add(u64 add_samples, u64 add_counter) {
        samples += add_samples;
        counter += add_counter;
    }
};

typedef std::pair<std::string, MethodSample> NamedMethodSample;

static bool sortByCounter(const NamedMethodSample& a, const NamedMethodSample& b) {
    return a.second.counter > b.second.counter;
}


static inline int noNativeStack(EventType event_type) {
    return (1 << event_type) & ((1 << INSTRUMENTED_METHOD) | (1 << LOCK_SAMPLE) | (1 << PARK_SAMPLE));
}

static inline bool isVTableStub(const char* name) {
    return name[0] && strcmp(name + 1, "table stub") == 0;
}

static inline int makeFrame(ASGCT_CallFrame* frames, jint type, jmethodID id) {
    frames[0].bci = type;
    frames[0].method_id = id;
    return 1;
}

static inline int makeFrame(ASGCT_CallFrame* frames, jint type, uintptr_t id) {
    return makeFrame(frames, type, (jmethodID)id);
}

static inline int makeFrame(ASGCT_CallFrame* frames, jint type, const char* id) {
    return makeFrame(frames, type, (jmethodID)id);
}


// Avoid syscall when possible
static inline int fastThreadId() {
    VMThread* vm_thread;
    if (VMStructs::hasNativeThreadId() && (vm_thread = VMThread::current()) != NULL) {
        int thread_id = vm_thread->osThreadId();
        if (thread_id > 0) {
            return thread_id;
        }
    }
    return OS::threadId();
}


void Profiler::addJavaMethod(const void* address, int length, jmethodID method) {
    CodeHeap::updateBounds(address, (const char*)address + length);
}

void Profiler::addRuntimeStub(const void* address, int length, const char* name) {
    _stubs_lock.lock();
    _runtime_stubs.add(address, length, name, true);
    _stubs_lock.unlock();

    if (strcmp(name, "call_stub") == 0) {
        _call_stub_begin = address;
        _call_stub_end = (const char*)address + length;
    }

    CodeHeap::updateBounds(address, (const char*)address + length);
}

void Profiler::onThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    if (_thread_filter.enabled()) {
        _thread_filter.remove(OS::threadId());
    }
    updateThreadName(jvmti, jni, thread);
}

void Profiler::onThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    if (_thread_filter.enabled()) {
        _thread_filter.remove(OS::threadId());
    }
    updateThreadName(jvmti, jni, thread);
}

void Profiler::onGarbageCollectionFinish() {
    // Called during GC pause, do not use JNI
    __sync_fetch_and_add(&_gc_id, 1);
}

const char* Profiler::asgctError(int code) {
    switch (code) {
        case ticks_no_Java_frame:
        case ticks_unknown_not_Java:
            // Not in Java context at all; this is not an error
            return NULL;
        case ticks_thread_exit:
            // The last Java frame has been popped off, only native frames left
            return NULL;
        case ticks_GC_active:
            return "GC_active";
        case ticks_unknown_Java:
            return "unknown_Java";
        case ticks_not_walkable_Java:
            return "not_walkable_Java";
        case ticks_not_walkable_not_Java:
            return "not_walkable_not_Java";
        case ticks_deopt:
            return "deoptimization";
        case ticks_safepoint:
            return "safepoint";
        case ticks_skipped:
            return "skipped";
        case ticks_unknown_state:
            // Zing sometimes returns it
            return "unknown_state";
        default:
            // Should not happen
            return "unexpected_state";
    }
}

inline u32 Profiler::getLockIndex(int tid) {
    u32 lock_index = tid;
    lock_index ^= lock_index >> 8;
    lock_index ^= lock_index >> 4;
    return lock_index % CONCURRENCY_LEVEL;
}

void Profiler::updateSymbols(bool kernel_symbols) {
    Symbols::parseLibraries(&_native_libs, kernel_symbols);
}

void Profiler::mangle(const char* name, char* buf, size_t size) {
    char* buf_end = buf + size;
    strcpy(buf, "_ZN");
    buf += 3;

    const char* c;
    while ((c = strstr(name, "::")) != NULL && buf + (c - name) + 4 < buf_end) {
        buf += snprintf(buf, buf_end - buf, "%d", (int)(c - name));
        memcpy(buf, name, c - name);
        buf += c - name;
        name = c + 2;
    }

    if (buf < buf_end) {
        snprintf(buf, buf_end - buf, "%d%sE*", (int)strlen(name), name);
    }
    buf_end[-1] = 0;
}

const void* Profiler::resolveSymbol(const char* name) {
    char mangled_name[256];
    if (strstr(name, "::") != NULL) {
        mangle(name, mangled_name, sizeof(mangled_name));
        name = mangled_name;
    }

    size_t len = strlen(name);
    int native_lib_count = _native_libs.count();
    if (len > 0 && name[len - 1] == '*') {
        for (int i = 0; i < native_lib_count; i++) {
            const void* address = _native_libs[i]->findSymbolByPrefix(name, len - 1);
            if (address != NULL) {
                return address;
            }
        }
    } else {
        for (int i = 0; i < native_lib_count; i++) {
            const void* address = _native_libs[i]->findSymbol(name);
            if (address != NULL) {
                return address;
            }
        }
    }

    return NULL;
}

// For BCI_NATIVE_FRAME, library index is encoded ahead of the symbol name
const char* Profiler::getLibraryName(const char* native_symbol) {
    short lib_index = NativeFunc::libIndex(native_symbol);
    if (lib_index >= 0 && lib_index < _native_libs.count()) {
        const char* s = _native_libs[lib_index]->name();
        if (s != NULL) {
            const char* p = strrchr(s, '/');
            return p != NULL ? p + 1 : s;
        }
    }
    return NULL;
}

CodeCache* Profiler::findJvmLibrary(const char* lib_name) {
    return VM::isOpenJ9() ? findLibraryByName(lib_name) : VMStructs::libjvm();
}

CodeCache* Profiler::findLibraryByName(const char* lib_name) {
    const size_t lib_name_len = strlen(lib_name);
    const int native_lib_count = _native_libs.count();
    for (int i = 0; i < native_lib_count; i++) {
        const char* s = _native_libs[i]->name();
        if (s != NULL) {
            const char* p = strrchr(s, '/');
            if (p != NULL && strncmp(p + 1, lib_name, lib_name_len) == 0) {
                return _native_libs[i];
            }
        }
    }
    return NULL;
}

CodeCache* Profiler::findLibraryByAddress(const void* address) {
    const int native_lib_count = _native_libs.count();
    for (int i = 0; i < native_lib_count; i++) {
        if (_native_libs[i]->contains(address)) {
            return _native_libs[i];
        }
    }
    return NULL;
}

const char* Profiler::findNativeMethod(const void* address) {
    CodeCache* lib = findLibraryByAddress(address);
    return lib == NULL ? NULL : lib->binarySearch(address);
}

CodeBlob* Profiler::findRuntimeStub(const void* address) {
    return _runtime_stubs.findBlobByAddress(address);
}

bool Profiler::isAddressInCode(const void* pc) {
    if (CodeHeap::contains(pc)) {
        return CodeHeap::findNMethod(pc) != NULL && !(pc >= _call_stub_begin && pc < _call_stub_end);
    } else {
        return findLibraryByAddress(pc) != NULL;
    }
}

jmethodID Profiler::getCurrentCompileTask() {
    VMThread* vm_thread = VMThread::current();
    if (vm_thread != NULL) {
        VMMethod* method = vm_thread->compiledMethod();
        if (method != NULL) {
            return method->id();
        }
    }
    return NULL;
}

int Profiler::getNativeTrace(void* ucontext, ASGCT_CallFrame* frames, EventType event_type, int tid, StackContext* java_ctx) {
    const void* callchain[MAX_NATIVE_FRAMES];
    int native_frames;

    if (_cstack == CSTACK_NO || noNativeStack(event_type)) {
        return 0;
    }

    // Use PerfEvents stack walker for execution samples, or basic stack walker for other events
    if (event_type == PERF_SAMPLE) {
        native_frames = PerfEvents::walk(tid, ucontext, callchain, MAX_NATIVE_FRAMES, java_ctx);
    } else if (_cstack == CSTACK_VM) {
        return 0;
    } else if (_cstack == CSTACK_DWARF) {
        native_frames = StackWalker::walkDwarf(ucontext, callchain, MAX_NATIVE_FRAMES, java_ctx);
    } else {
        native_frames = StackWalker::walkFP(ucontext, callchain, MAX_NATIVE_FRAMES, java_ctx);
    }

    return convertNativeTrace(native_frames, callchain, frames, event_type);
}

int Profiler::convertNativeTrace(int native_frames, const void** callchain, ASGCT_CallFrame* frames, EventType event_type) {
    int depth = 0;
    jmethodID prev_method = NULL;

    for (int i = 0; i < native_frames; i++) {
        const char* current_method_name = findNativeMethod(callchain[i]);
        char mark;
        if (current_method_name != NULL && (mark = NativeFunc::mark(current_method_name)) != 0) {
            if (mark == MARK_VM_RUNTIME && event_type >= ALLOC_SAMPLE) {
                // Skip all internal frames above VM runtime entry for allocation samples
                depth = 0;
                continue;
            } else if (mark == MARK_INTERPRETER) {
                // This is C++ interpreter frame, this and later frames should be reported
                // as Java frames returned by AGCT. Terminate the scan here.
                return depth;
            } else if (mark == MARK_COMPILER_ENTRY && _features.comp_task) {
                // Insert current compile task as a pseudo Java frame
                jmethodID compile_task = getCurrentCompileTask();
                if (compile_task != NULL) {
                    frames[depth].bci = 0;
                    frames[depth].method_id = compile_task;
                    depth++;
                }
            }
        }

        jmethodID current_method = (jmethodID)current_method_name;
        if (current_method == prev_method && _cstack == CSTACK_LBR) {
            // Skip duplicates in LBR stack, where branch_stack[N].from == branch_stack[N+1].to
            prev_method = NULL;
        } else {
            frames[depth].bci = BCI_NATIVE_FRAME;
            frames[depth].method_id = prev_method = current_method;
            depth++;
        }
    }

    return depth;
}

int Profiler::getJavaTraceAsync(void* ucontext, ASGCT_CallFrame* frames, int max_depth, StackContext* java_ctx) {
    // Workaround for JDK-8132510: it's not safe to call GetEnv() inside a signal handler
    // since JDK 9, so we do it only for threads already registered in ThreadLocalStorage
    VMThread* vm_thread = VMThread::current();
    if (vm_thread == NULL) {
        return 0;
    }

    JNIEnv* jni = VM::jni();
    if (jni == NULL) {
        // Not a Java thread
        return 0;
    }

    StackFrame frame(ucontext);
    uintptr_t saved_pc, saved_sp, saved_fp;
    if (ucontext != NULL) {
        saved_pc = frame.pc();
        saved_sp = frame.sp();
        saved_fp = frame.fp();
    }

    if (_features.unwind_native && vm_thread->inJava()) {
        if (saved_pc >= (uintptr_t)_call_stub_begin && saved_pc < (uintptr_t)_call_stub_end) {
            // call_stub is unsafe to walk
            frames->bci = BCI_ERROR;
            frames->method_id = (jmethodID)"call_stub";
            return 1;
        }
        if (DWARF_SUPPORTED && java_ctx->sp != 0) {
            // If a thread is in Java state, unwind manually to the last known Java frame,
            // since JVM does not always correctly unwind native frames
            frame.restore((uintptr_t)java_ctx->pc, java_ctx->sp, java_ctx->fp);
        }
    }

    JitWriteProtection jit(false);
    ASGCT_CallTrace trace = {jni, 0, frames};
    VM::_asyncGetCallTrace(&trace, max_depth, ucontext);

    if (trace.num_frames > 0) {
        frame.restore(saved_pc, saved_sp, saved_fp);
        return trace.num_frames;
    }

    if ((trace.num_frames == ticks_unknown_Java || trace.num_frames == ticks_not_walkable_Java) && _features.unknown_java && ucontext != NULL) {
        CodeBlob* stub = NULL;
        _stubs_lock.lockShared();
        if (_runtime_stubs.contains((const void*)frame.pc())) {
            stub = findRuntimeStub((const void*)frame.pc());
        }
        _stubs_lock.unlockShared();

        if (stub != NULL) {
            if (_cstack != CSTACK_NO) {
                if (_features.vtable_target && isVTableStub(stub->_name)) {
                    uintptr_t receiver = frame.jarg0();
                    if (receiver != 0) {
                        VMSymbol* symbol = VMKlass::fromOop(receiver)->name();
                        u32 class_id = classMap()->lookup(symbol->body(), symbol->length());
                        max_depth -= makeFrame(trace.frames++, BCI_ALLOC, class_id);
                    }
                }
                max_depth -= makeFrame(trace.frames++, BCI_NATIVE_FRAME, stub->_name);
            }
            if (_features.unwind_stub && frame.unwindStub((instruction_t*)stub->_start, stub->_name)
                    && isAddressInCode((const void*)frame.pc())) {
                java_ctx->pc = (const void*)frame.pc();
                VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
            }
        } else if (VMStructs::hasMethodStructs()) {
            NMethod* nmethod = CodeHeap::findNMethod((const void*)frame.pc());
            if (nmethod != NULL && nmethod->isNMethod() && nmethod->isAlive()) {
                VMMethod* method = nmethod->method();
                if (method != NULL) {
                    jmethodID method_id = method->id();
                    if (method_id != NULL) {
                        max_depth -= makeFrame(trace.frames++, 0, method_id);
                    }
                    if (_features.unwind_comp && frame.unwindCompiled(nmethod)
                            && isAddressInCode((const void*)frame.pc())) {
                        VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
                    }
                    if (_features.probe_sp && trace.num_frames < 0) {
                        if (method_id != NULL) {
                            trace.frames--;
                        }
                        for (int i = 0; trace.num_frames < 0 && i < PROBE_SP_LIMIT; i++) {
                            frame.sp() += sizeof(void*);
                            VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
                        }
                    }
                }
            } else if (nmethod != NULL) {
                if (_cstack != CSTACK_NO) {
                    max_depth -= makeFrame(trace.frames++, BCI_NATIVE_FRAME, nmethod->name());
                }
                if (_features.unwind_stub && frame.unwindStub(NULL, nmethod->name())
                        && isAddressInCode((const void*)frame.pc())) {
                    VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
                }
            }
        }
    } else if (trace.num_frames == ticks_unknown_not_Java && _features.java_anchor) {
        uintptr_t& sp = vm_thread->lastJavaSP();
        uintptr_t& pc = vm_thread->lastJavaPC();
        if (sp != 0 && pc == 0) {
            // We have the last Java frame anchor, but it is not marked as walkable.
            // Make it walkable here
            pc = ((uintptr_t*)sp)[-1];

            NMethod* m = CodeHeap::findNMethod((const void*)pc);
            if (m != NULL) {
                // AGCT fails if the last Java frame is a Runtime Stub with an invalid _frame_complete_offset.
                // In this case we patch _frame_complete_offset manually
                if (!m->isNMethod() && m->frameSize() > 0 && m->frameCompleteOffset() == -1) {
                    m->setFrameCompleteOffset(0);
                }
                VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
            } else if (findLibraryByAddress((const void*)pc) != NULL) {
                VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
            }

            pc = 0;
        }
    } else if (trace.num_frames == ticks_not_walkable_not_Java && _features.java_anchor) {
        uintptr_t& sp = vm_thread->lastJavaSP();
        uintptr_t& pc = vm_thread->lastJavaPC();
        if (sp != 0 && pc != 0) {
            // Similar to the above: last Java frame is set,
            // but points to a Runtime Stub with an invalid _frame_complete_offset
            NMethod* m = CodeHeap::findNMethod((const void*)pc);
            if (m != NULL && !m->isNMethod() && m->frameSize() > 0 && m->frameCompleteOffset() == -1) {
                m->setFrameCompleteOffset(0);
                VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
            }
        }
    } else if (trace.num_frames == ticks_GC_active && _features.gc_traces) {
        if (vm_thread->lastJavaSP() == 0) {
            // Do not add 'GC_active' for threads with no Java frames, e.g. Compiler threads
            frame.restore(saved_pc, saved_sp, saved_fp);
            return 0;
        }
    }

    frame.restore(saved_pc, saved_sp, saved_fp);

    if (trace.num_frames > 0) {
        return trace.num_frames + (trace.frames - frames);
    }

    const char* err_string = asgctError(trace.num_frames);
    if (err_string == NULL) {
        // No Java stack, because thread is not in Java context
        return 0;
    }

    atomicInc(_failures[-trace.num_frames]);
    trace.frames->bci = BCI_ERROR;
    trace.frames->method_id = (jmethodID)err_string;
    return trace.frames - frames + 1;
}

int Profiler::getJavaTraceJvmti(jvmtiFrameInfo* jvmti_frames, ASGCT_CallFrame* frames, int start_depth, int max_depth) {
    int num_frames;
    if (VM::jvmti()->GetStackTrace(NULL, start_depth, _max_stack_depth, jvmti_frames, &num_frames) == 0 && num_frames > 0) {
        return convertFrames(jvmti_frames, frames, num_frames);
    }
    return 0;
}

int Profiler::getJavaTraceInternal(jvmtiFrameInfo* jvmti_frames, ASGCT_CallFrame* frames, int max_depth) {
    // We cannot call pure JVM TI here, because it assumes _thread_in_native state,
    // but allocation events happen in _thread_in_vm state,
    // see https://github.com/async-profiler/async-profiler/issues/64
    JNIEnv* jni = VM::jni();
    if (jni == NULL) {
        return 0;
    }

    JitWriteProtection jit(false);
    VMThread* vm_thread = VMThread::fromEnv(jni);
    int num_frames;
    if (VMStructs::_get_stack_trace(NULL, vm_thread, 0, max_depth, jvmti_frames, &num_frames) == 0 && num_frames > 0) {
        return convertFrames(jvmti_frames, frames, num_frames);
    }
    return 0;
}

inline int Profiler::convertFrames(jvmtiFrameInfo* jvmti_frames, ASGCT_CallFrame* frames, int num_frames) {
    // Convert to AsyncGetCallTrace format.
    // Note: jvmti_frames and frames may overlap.
    for (int i = 0; i < num_frames; i++) {
        jint bci = jvmti_frames[i].location;
        frames[i].method_id = jvmti_frames[i].method;
        frames[i].bci = bci;
    }
    return num_frames;
}

void Profiler::fillFrameTypes(ASGCT_CallFrame* frames, int num_frames, NMethod* nmethod) {
    if (nmethod->isNMethod() && nmethod->isAlive()) {
        VMMethod* method = nmethod->method();
        if (method == NULL) {
            return;
        }

        jmethodID current_method_id = method->id();
        if (current_method_id == NULL) {
            return;
        }

        // If the top frame is a runtime stub, skip it
        if (num_frames > 0 && frames[0].bci == BCI_NATIVE_FRAME) {
            frames++;
            num_frames--;
        }

        // Mark current_method as COMPILED and frames above current_method as INLINED
        for (int i = 0; i < num_frames; i++) {
            if (frames[i].method_id == NULL || frames[i].bci <= BCI_NATIVE_FRAME) {
                break;
            }
            if (frames[i].method_id == current_method_id) {
                int level = nmethod->level();
                frames[i].bci = FrameType::encode(level >= 1 && level <= 3 ? FRAME_C1_COMPILED : FRAME_JIT_COMPILED, frames[i].bci);
                for (int j = 0; j < i; j++) {
                    frames[j].bci = FrameType::encode(FRAME_INLINED, frames[j].bci);
                }
                break;
            }
        }
    } else if (nmethod->isInterpreter()) {
        // Mark the first Java frame as INTERPRETED
        for (int i = 0; i < num_frames; i++) {
            if (frames[i].bci > BCI_NATIVE_FRAME) {
                frames[i].bci = FrameType::encode(FRAME_INTERPRETED, frames[i].bci);
                break;
            }
        }
    }
}

u64 Profiler::recordSample(void* ucontext, u64 counter, EventType event_type, Event* event) {
    atomicInc(_total_samples);

    int tid = fastThreadId();
    u32 lock_index = getLockIndex(tid);
    if (!_locks[lock_index].tryLock() &&
        !_locks[lock_index = (lock_index + 1) % CONCURRENCY_LEVEL].tryLock() &&
        !_locks[lock_index = (lock_index + 2) % CONCURRENCY_LEVEL].tryLock())
    {
        // Too many concurrent signals already
        atomicInc(_failures[-ticks_skipped]);

        if (event_type == PERF_SAMPLE) {
            // Need to reset PerfEvents ring buffer, even though we discard the collected trace
            PerfEvents::resetBuffer(tid);
        }
        return 0;
    }

    ASGCT_CallFrame* frames = _calltrace_buffer[lock_index]->_asgct_frames;
    jvmtiFrameInfo* jvmti_frames = _calltrace_buffer[lock_index]->_jvmti_frames;

    int num_frames = 0;
    if (_add_event_frame && event_type >= ALLOC_SAMPLE && event_type <= PARK_SAMPLE) {
        u32 class_id = ((EventWithClassId*)event)->_class_id;
        if (class_id != 0) {
            // Convert event_type to frame_type, e.g. ALLOC_SAMPLE -> BCI_ALLOC
            jint frame_type = BCI_ALLOC - (event_type - ALLOC_SAMPLE);
            num_frames = makeFrame(frames, frame_type, class_id);
        }
    }

    if (_features.pc_addr && event_type <= EXECUTION_SAMPLE && ucontext != NULL) {
        num_frames += makeFrame(frames + num_frames, BCI_ADDRESS, StackFrame(ucontext).pc());
    }

    StackContext java_ctx = {0};
    num_frames += getNativeTrace(ucontext, frames + num_frames, event_type, tid, &java_ctx);

    if (_cstack == CSTACK_VM) {
        num_frames += StackWalker::walkVM(ucontext, frames + num_frames, _max_stack_depth);
    } else if (event_type <= EXECUTION_SAMPLE) {
        // Async events
        int java_frames = getJavaTraceAsync(ucontext, frames + num_frames, _max_stack_depth, &java_ctx);
        if (java_frames > 0 && java_ctx.pc != NULL && VMStructs::hasMethodStructs()) {
            NMethod* nmethod = CodeHeap::findNMethod(java_ctx.pc);
            if (nmethod != NULL) {
                fillFrameTypes(frames + num_frames, java_frames, nmethod);
            }
        }
        num_frames += java_frames;
    } else if (event_type >= ALLOC_SAMPLE && event_type <= ALLOC_OUTSIDE_TLAB && _alloc_engine == &alloc_tracer) {
        if (VMStructs::_get_stack_trace != NULL) {
            // Object allocation in HotSpot happens at known places where it is safe to call JVM TI,
            // but not directly, since the thread is in_vm rather than in_native
            num_frames += getJavaTraceInternal(jvmti_frames + num_frames, frames + num_frames, _max_stack_depth);
        } else {
            num_frames += getJavaTraceAsync(ucontext, frames + num_frames, _max_stack_depth, &java_ctx);
        }
    } else {
        // Lock events and instrumentation events can safely call synchronous JVM TI stack walker.
        // Skip Instrument.recordSample() method
        int start_depth = event_type == INSTRUMENTED_METHOD ? 1 : 0;
        num_frames += getJavaTraceJvmti(jvmti_frames + num_frames, frames + num_frames, start_depth, _max_stack_depth);
    }

    if (num_frames == 0) {
        num_frames += makeFrame(frames + num_frames, BCI_ERROR, "no_Java_frame");
    }

    if (_add_thread_frame) {
        num_frames += makeFrame(frames + num_frames, BCI_THREAD_ID, tid);
    }
    if (_add_sched_frame) {
        num_frames += makeFrame(frames + num_frames, BCI_ERROR, OS::schedPolicy(0));
    }

    u32 call_trace_id = _call_trace_storage.put(num_frames, frames, counter);
    _jfr.recordEvent(lock_index, tid, call_trace_id, event_type, event);

    _locks[lock_index].unlock();
    return (u64)tid << 32 | call_trace_id;
}

void Profiler::recordExternalSample(u64 counter, int tid, EventType event_type, Event* event, int num_frames, ASGCT_CallFrame* frames) {
    atomicInc(_total_samples);

    if (_add_thread_frame) {
        num_frames += makeFrame(frames + num_frames, BCI_THREAD_ID, tid);
    }
    if (_add_sched_frame) {
        num_frames += makeFrame(frames + num_frames, BCI_ERROR, OS::schedPolicy(tid));
    }

    u32 call_trace_id = _call_trace_storage.put(num_frames, frames, counter);

    u32 lock_index = getLockIndex(tid);
    if (!_locks[lock_index].tryLock() &&
        !_locks[lock_index = (lock_index + 1) % CONCURRENCY_LEVEL].tryLock() &&
        !_locks[lock_index = (lock_index + 2) % CONCURRENCY_LEVEL].tryLock())
    {
        // Too many concurrent signals already
        atomicInc(_failures[-ticks_skipped]);
        return;
    }

    _jfr.recordEvent(lock_index, tid, call_trace_id, event_type, event);

    _locks[lock_index].unlock();
}

void Profiler::recordExternalSample(u64 counter, int tid, EventType event_type, Event* event, u32 call_trace_id) {
    _call_trace_storage.add(call_trace_id, counter);

    u32 lock_index = getLockIndex(tid);
    if (!_locks[lock_index].tryLock() &&
        !_locks[lock_index = (lock_index + 1) % CONCURRENCY_LEVEL].tryLock() &&
        !_locks[lock_index = (lock_index + 2) % CONCURRENCY_LEVEL].tryLock())
    {
        return;
    }

    _jfr.recordEvent(lock_index, tid, call_trace_id, event_type, event);

    _locks[lock_index].unlock();
}

void Profiler::recordEventOnly(EventType event_type, Event* event) {
    if (!_jfr.active()) {
        return;
    }

    int tid = fastThreadId();
    u32 lock_index = getLockIndex(tid);
    if (!_locks[lock_index].tryLock() &&
        !_locks[lock_index = (lock_index + 1) % CONCURRENCY_LEVEL].tryLock() &&
        !_locks[lock_index = (lock_index + 2) % CONCURRENCY_LEVEL].tryLock())
    {
        return;
    }

    _jfr.recordEvent(lock_index, tid, 0, event_type, event);

    _locks[lock_index].unlock();
}

void Profiler::writeLog(LogLevel level, const char* message) {
    _jfr.recordLog(level, message, strlen(message));
}

void Profiler::writeLog(LogLevel level, const char* message, size_t len) {
    _jfr.recordLog(level, message, len);
}

void* Profiler::dlopen_hook(const char* filename, int flags) {
    void* result = dlopen(filename, flags);
    if (result != NULL) {
        instance()->updateSymbols(false);
    }
    return result;
}

void Profiler::switchLibraryTrap(bool enable) {
    if (_dlopen_entry != NULL) {
        void* impl = enable ? (void*)dlopen_hook : (void*)dlopen;
        __atomic_store_n(_dlopen_entry, impl, __ATOMIC_RELEASE);
    }
}

Error Profiler::installTraps(const char* begin, const char* end) {
    const void* begin_addr = NULL;
    if (begin != NULL && (begin_addr = resolveSymbol(begin)) == NULL) {
        return Error("Begin address not found");
    }

    const void* end_addr = NULL;
    if (end != NULL && (end_addr = resolveSymbol(end)) == NULL) {
        return Error("End address not found");
    }

    _begin_trap.assign(begin_addr);
    _end_trap.assign(end_addr);

    if (_begin_trap.entry() == 0) {
        _engine->enableEvents(true);
    } else {
        _engine->enableEvents(false);
        if (!_begin_trap.install()) {
            return Error("Cannot install begin breakpoint");
        }
    }

    return Error::OK;
}

void Profiler::uninstallTraps() {
    _begin_trap.uninstall();
    _end_trap.uninstall();
    _engine->enableEvents(false);
}

void Profiler::trapHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    StackFrame frame(ucontext);

    if (_begin_trap.covers(frame.pc())) {
        profiling_window._start_time = TSC::ticks();
        _engine->enableEvents(true);
        _begin_trap.uninstall();
        _end_trap.install();
        frame.pc() = _begin_trap.entry();
    } else if (_end_trap.covers(frame.pc())) {
        _engine->enableEvents(false);
        _end_trap.uninstall();
        profiling_window._end_time = TSC::ticks();
        recordEventOnly(PROFILING_WINDOW, &profiling_window);
        _begin_trap.install();
        frame.pc() = _end_trap.entry();
    } else if (orig_trapHandler != NULL) {
        orig_trapHandler(signo, siginfo, ucontext);
    }
}

void Profiler::segvHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    StackFrame frame(ucontext);
    uintptr_t pc = frame.pc();

    uintptr_t length = SafeAccess::skipLoad(pc);
    if (length > 0) {
        // Skip the fault instruction, as if it successfully loaded NULL
        frame.pc() += length;
        frame.retval() = 0;
        return;
    }

    length = SafeAccess::skipLoadArg(pc);
    if (length > 0) {
        // Act as if the load returned default_value argument
        frame.pc() += length;
        frame.retval() = frame.arg1();
        return;
    }

    StackWalker::checkFault();

    // Workaround for JDK-8313796. Setting cstack=dwarf also helps
    if (VMStructs::isInterpretedFrameValidFunc((const void*)pc) && frame.skipFaultInstruction()) {
        return;
    }

    if (WX_MEMORY && Trap::isFaultInstruction(pc)) {
        return;
    }

    orig_segvHandler(signo, siginfo, ucontext);
}

void Profiler::wakeupHandler(int signo) {
    // Dummy handler for interrupting syscalls
}

void Profiler::setupSignalHandlers() {
    SigAction prev_handler = OS::installSignalHandler(SIGTRAP, AllocTracer::trapHandler);
    if (prev_handler == AllocTracer::trapHandler) {
        // Handlers already configured
        return;
    } else if (prev_handler != (void*)SIG_DFL && prev_handler != (void*)SIG_IGN) {
        orig_trapHandler = prev_handler;
    }

    if (VM::hotspot_version() > 0 || !VM::loaded()) {
        // HotSpot tolerates interposed SIGSEGV/SIGBUS handler; other JVMs probably not
        orig_segvHandler = OS::replaceCrashHandler(segvHandler);
    }

    OS::installSignalHandler(WAKEUP_SIGNAL, NULL, wakeupHandler);
}

void Profiler::setThreadInfo(int tid, const char* name, jlong java_thread_id) {
    MutexLocker ml(_thread_names_lock);
    _thread_names[tid] = name;
    _thread_ids[tid] = java_thread_id;
}

void Profiler::updateThreadName(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    if (_update_thread_names) {
        JitWriteProtection jit(true);  // workaround for JDK-8262896
        jvmtiThreadInfo thread_info;
        int native_thread_id = VMThread::nativeThreadId(jni, thread);
        if (native_thread_id >= 0 && jvmti->GetThreadInfo(thread, &thread_info) == 0) {
            jlong java_thread_id = VMThread::javaThreadId(jni, thread);
            setThreadInfo(native_thread_id, thread_info.name, java_thread_id);
            jvmti->Deallocate((unsigned char*)thread_info.name);
        }
    }
}

void Profiler::updateJavaThreadNames() {
    if (_update_thread_names && VM::loaded()) {
        jvmtiEnv* jvmti = VM::jvmti();
        jint thread_count;
        jthread* thread_objects;
        if (jvmti->GetAllThreads(&thread_count, &thread_objects) != 0) {
            return;
        }

        JNIEnv* jni = VM::jni();
        for (int i = 0; i < thread_count; i++) {
            updateThreadName(jvmti, jni, thread_objects[i]);
        }

        jvmti->Deallocate((unsigned char*)thread_objects);
    }
}

void Profiler::updateNativeThreadNames() {
    if (_update_thread_names) {
        ThreadList* thread_list = OS::listThreads();
        char name_buf[64];

        for (int tid; (tid = thread_list->next()) != -1; ) {
            MutexLocker ml(_thread_names_lock);
            std::map<int, std::string>::iterator it = _thread_names.lower_bound(tid);
            if (it == _thread_names.end() || it->first != tid) {
                if (OS::threadName(tid, name_buf, sizeof(name_buf))) {
                    _thread_names.insert(it, std::map<int, std::string>::value_type(tid, name_buf));
                }
            }
        }

        delete thread_list;
    }
}

bool Profiler::excludeTrace(FrameName* fn, CallTrace* trace) {
    bool checkInclude = fn->hasIncludeList();
    bool checkExclude = fn->hasExcludeList();
    if (!(checkInclude || checkExclude)) {
        return false;
    }

    for (int i = 0; i < trace->num_frames; i++) {
        const char* frame_name = fn->name(trace->frames[i], true);
        if (checkExclude && fn->exclude(frame_name)) {
            return true;
        }
        if (checkInclude && fn->include(frame_name)) {
            checkInclude = false;
            if (!checkExclude) break;
        }
    }

    return checkInclude;
}

Engine* Profiler::selectEngine(const char* event_name) {
    if (event_name == NULL) {
        return &noop_engine;
    } else if (strcmp(event_name, EVENT_CPU) == 0) {
        return PerfEvents::supported() ? (Engine*)&perf_events : (Engine*)&wall_clock;
    } else if (strcmp(event_name, EVENT_WALL) == 0) {
        return VM::isOpenJ9() ? (Engine*)&j9_wall_clock : (Engine*)&wall_clock;
    } else if (strcmp(event_name, EVENT_CTIMER) == 0) {
        return &ctimer;
    } else if (strcmp(event_name, EVENT_ITIMER) == 0) {
        return &itimer;
    } else if (strchr(event_name, '.') != NULL && strchr(event_name, ':') == NULL) {
        return &instrument;
    } else {
        return &perf_events;
    }
}

Engine* Profiler::selectAllocEngine(long alloc_interval, bool live) {
    if (VM::canSampleObjects()) {
        return &object_sampler;
    } else if (VM::isOpenJ9()) {
        return &j9_object_sampler;
    } else {
        return &alloc_tracer;
    }
}

Engine* Profiler::activeEngine() {
    switch (_event_mask) {
        case EM_ALLOC:
            return _alloc_engine;
        case EM_LOCK:
            return &lock_tracer;
        case EM_WALL:
            return &wall_clock;
        default:
            return _engine;
    }
}

Error Profiler::checkJvmCapabilities() {
    if (VM::loaded()) {
        if (!VMStructs::hasJavaThreadId()) {
            return Error("Could not find Thread ID field. Unsupported JVM?");
        }

        if (VMThread::key() < 0) {
            return Error("Could not find VMThread bridge. Unsupported JVM?");
        }

        if (_dlopen_entry == NULL) {
            CodeCache* lib = findJvmLibrary("libj9prt");
            if (lib == NULL || (_dlopen_entry = lib->findImport(im_dlopen)) == NULL) {
                return Error("Could not set dlopen hook. Unsupported JVM?");
            }
        }

        if (!VMStructs::libjvm()->hasDebugSymbols()) {
            Log::warn("Install JVM debug symbols to improve profile accuracy");
        }
    }

    return Error::OK;
}

Error Profiler::start(Arguments& args, bool reset) {
    MutexLocker ml(_state_lock);
    if (_state > IDLE) {
        return Error("Profiler already started");
    }

    Error error = checkJvmCapabilities();
    if (error) {
        return error;
    }

    _event_mask = (args._event != NULL ? EM_CPU : 0) |
                  (args._alloc >= 0 ? EM_ALLOC : 0) |
                  (args._lock >= 0 ? EM_LOCK : 0) |
                  (args._wall >= 0 ? EM_WALL : 0);
    if (_event_mask == 0) {
        return Error("No profiling events specified");
    } else if ((_event_mask & (_event_mask - 1)) && args._output != OUTPUT_JFR) {
        return Error("Only JFR output supports multiple events");
    } else if (!VM::loaded() && (_event_mask & (EM_ALLOC | EM_LOCK))) {
        return Error("Profiling event is not supported with non-Java processes");
    }

    if (args._fdtransfer) {
        if (!FdTransferClient::connectToServer(args._fdtransfer_path)) {
            return Error("Failed to initialize FdTransferClient");
        }
    }

    // Save the arguments for shutdown or restart
    args.save();

    if (reset || _start_time == 0) {
        // Reset counters
        _total_samples = 0;
        memset(_failures, 0, sizeof(_failures));

        // Reset dictionaries and bitmaps
        lockAll();
        _class_map.clear();
        _thread_filter.clear();
        _call_trace_storage.clear();
        // Make sure frame structure is consistent throughout the entire recording
        _add_event_frame = args._output != OUTPUT_JFR;
        _add_thread_frame = args._threads && args._output != OUTPUT_JFR;
        _add_sched_frame = args._sched;
        unlockAll();

        // Reset thread names and IDs
        MutexLocker ml(_thread_names_lock);
        _thread_names.clear();
        _thread_ids.clear();
    }

    // (Re-)allocate calltrace buffers
    if (_max_stack_depth != args._jstackdepth) {
        _max_stack_depth = args._jstackdepth;
        size_t buffer_size = (_max_stack_depth + MAX_NATIVE_FRAMES + RESERVED_FRAMES) * sizeof(CallTraceBuffer);

        for (int i = 0; i < CONCURRENCY_LEVEL; i++) {
            free(_calltrace_buffer[i]);
            _calltrace_buffer[i] = (CallTraceBuffer*)malloc(buffer_size);
            if (_calltrace_buffer[i] == NULL) {
                _max_stack_depth = 0;
                return Error("Not enough memory to allocate stack trace buffers (try smaller jstackdepth)");
            }
        }
    }

    _features = args._features;
    if (VM::hotspot_version() < 8) {
        _features.java_anchor = 0;
        _features.gc_traces = 0;
    }
    if (!VMStructs::hasClassNames()) {
        _features.vtable_target = 0;
    }
    if (!VMStructs::hasCompilerStructs()) {
        _features.comp_task = 0;
    }

    _update_thread_names = args._threads || args._output == OUTPUT_JFR;
    _thread_filter.init(args._filter);

    _engine = selectEngine(args._event);
    if (_engine == &wall_clock && args._wall >= 0) {
        return Error("Cannot start wall clock with the selected event");
    }

    _cstack = args._cstack;
    if (_cstack == CSTACK_DWARF && !DWARF_SUPPORTED) {
        return Error("DWARF unwinding is not supported on this platform");
    } else if (_cstack == CSTACK_LBR && _engine != &perf_events) {
        return Error("Branch stack is supported only with PMU events");
    } else if (_cstack == CSTACK_VM && !VMStructs::hasStackStructs()) {
        return Error("VMStructs stack walking is not supported on this JVM/platform");
    }

    // Kernel symbols are useful only for perf_events without --all-user
    updateSymbols(_engine == &perf_events && args._ring != RING_USER);

    error = installTraps(args._begin, args._end);
    if (error) {
        return error;
    }

    switchLibraryTrap(true);

    if (args._output == OUTPUT_JFR) {
        error = _jfr.start(args, reset);
        if (error) {
            uninstallTraps();
            switchLibraryTrap(false);
            return error;
        }
    }

    error = _engine->start(args);
    if (error) {
        goto error1;
    }

    if (_event_mask & EM_ALLOC) {
        _alloc_engine = selectAllocEngine(args._alloc, args._live);
        error = _alloc_engine->start(args);
        if (error) {
            goto error2;
        }
    }
    if (_event_mask & EM_LOCK) {
        error = lock_tracer.start(args);
        if (error) {
            goto error3;
        }
    }
    if (_event_mask & EM_WALL) {
        error = wall_clock.start(args);
        if (error) {
            goto error4;
        }
    }

    switchThreadEvents(JVMTI_ENABLE);

    _state = RUNNING;
    _start_time = time(NULL);
    _epoch++;

    if (args._timeout != 0 || args._output == OUTPUT_JFR) {
        _stop_time = addTimeout(_start_time, args._timeout);
        startTimer();
    }

    return Error::OK;

error4:
    if (_event_mask & EM_LOCK) lock_tracer.stop();

error3:
    if (_event_mask & EM_ALLOC) _alloc_engine->stop();

error2:
    _engine->stop();

error1:
    uninstallTraps();
    switchLibraryTrap(false);

    lockAll();
    _jfr.stop();
    unlockAll();

    FdTransferClient::closePeer();
    return error;
}

Error Profiler::stop(bool restart) {
    MutexLocker ml(_state_lock);
    if (_state != RUNNING) {
        return Error("Profiler is not active");
    }

    uninstallTraps();

    if (_event_mask & EM_WALL) wall_clock.stop();
    if (_event_mask & EM_LOCK) lock_tracer.stop();
    if (_event_mask & EM_ALLOC) _alloc_engine->stop();

    _engine->stop();

    switchLibraryTrap(false);
    switchThreadEvents(JVMTI_DISABLE);
    updateJavaThreadNames();
    updateNativeThreadNames();

    // Make sure no periodic events sent after JFR stops
    stopTimer();

    // Acquire all spinlocks to avoid race with remaining signals
    lockAll();
    _jfr.stop();
    unlockAll();

    if (!restart) {
        FdTransferClient::closePeer();
    }

    _state = IDLE;
    return Error::OK;
}

Error Profiler::check(Arguments& args) {
    MutexLocker ml(_state_lock);
    if (_state > IDLE) {
        return Error("Profiler already started");
    }

    Error error = checkJvmCapabilities();

    if (!error && args._event != NULL) {
        _engine = selectEngine(args._event);
        error = _engine->check(args);
    }
    if (!error && args._alloc >= 0) {
        _alloc_engine = selectAllocEngine(args._alloc, args._live);
        error = _alloc_engine->check(args);
    }
    if (!error && args._lock >= 0) {
        error = lock_tracer.check(args);
    }

    if (!error) {
        if (args._wall >= 0 && _engine == &wall_clock) {
            return Error("Cannot start wall clock with the selected event");
        }

        if (args._cstack == CSTACK_DWARF && !DWARF_SUPPORTED) {
            return Error("DWARF unwinding is not supported on this platform");
        } else if (args._cstack == CSTACK_LBR && _engine != &perf_events) {
            return Error("Branch stack is supported only with PMU events");
        } else if (args._cstack == CSTACK_VM && !VMStructs::hasStackStructs()) {
            return Error("VMStructs stack walking is not supported on this JVM/platform");
        }
    }

    return error;
}

Error Profiler::flushJfr() {
    MutexLocker ml(_state_lock);
    if (_state != RUNNING) {
        return Error("Profiler is not active");
    }

    updateJavaThreadNames();
    updateNativeThreadNames();

    lockAll();
    _jfr.flush();
    unlockAll();

    return Error::OK;
}

Error Profiler::dump(Writer& out, Arguments& args) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE && _state != RUNNING) {
        return Error("Profiler has not started");
    }

    if (_state == RUNNING) {
        updateJavaThreadNames();
        updateNativeThreadNames();
    }

    switch (args._output) {
        case OUTPUT_COLLAPSED:
            dumpCollapsed(out, args);
            break;
        case OUTPUT_FLAMEGRAPH:
            dumpFlameGraph(out, args, false);
            break;
        case OUTPUT_TREE:
            dumpFlameGraph(out, args, true);
            break;
        case OUTPUT_TEXT:
            dumpText(out, args);
            break;
        case OUTPUT_JFR:
            if (_state == RUNNING) {
                lockAll();
                _jfr.flush();
                unlockAll();
            }
            break;
        default:
            return Error("No output format selected");
    }

    return Error::OK;
}

void Profiler::printUsedMemory(Writer& out) {
    size_t call_trace_storage = _call_trace_storage.usedMemory();
    size_t flight_recording = _jfr.usedMemory();
    size_t dictionaries = _class_map.usedMemory() + _symbol_map.usedMemory() + _thread_filter.usedMemory();

    size_t code_cache = _runtime_stubs.usedMemory();
    int native_lib_count = _native_libs.count();
    for (int i = 0; i < native_lib_count; i++) {
        code_cache += _native_libs[i]->usedMemory();
    }
    code_cache += native_lib_count * sizeof(CodeCache);

    char buf[1024];
    const size_t KB = 1024;
    snprintf(buf, sizeof(buf) - 1,
             "Call trace storage: %7zu KB\n"
             "  Flight recording: %7zu KB\n"
             "      Dictionaries: %7zu KB\n"
             "        Code cache: %7zu KB\n"
             "------------------------------\n"
             "             Total: %7zu KB\n",
             call_trace_storage / KB, flight_recording / KB, dictionaries / KB, code_cache / KB,
             (call_trace_storage + flight_recording + dictionaries + code_cache) / KB);
    out << buf;
}

void Profiler::lockAll() {
    for (int i = 0; i < CONCURRENCY_LEVEL; i++) _locks[i].lock();
}

void Profiler::unlockAll() {
    for (int i = 0; i < CONCURRENCY_LEVEL; i++) _locks[i].unlock();
}

void Profiler::switchThreadEvents(jvmtiEventMode mode) {
    if (_thread_events_state != mode && VM::loaded()) {
        jvmtiEnv* jvmti = VM::jvmti();
        jvmti->SetEventNotificationMode(mode, JVMTI_EVENT_THREAD_START, NULL);
        jvmti->SetEventNotificationMode(mode, JVMTI_EVENT_THREAD_END, NULL);
        _thread_events_state = mode;
    }
}

/*
 * Dump stacks in FlameGraph input format:
 * 
 * <frame>;<frame>;...;<topmost frame> <count>
 */
void Profiler::dumpCollapsed(Writer& out, Arguments& args) {
    FrameName fn(args, args._style | STYLE_NO_SEMICOLON, _epoch, _thread_names_lock, _thread_names);
    char buf[32];

    std::vector<CallTraceSample*> samples;
    _call_trace_storage.collectSamples(samples);

    for (std::vector<CallTraceSample*>::const_iterator it = samples.begin(); it != samples.end(); ++it) {
        CallTrace* trace = (*it)->acquireTrace();
        if (trace == NULL || excludeTrace(&fn, trace)) continue;

        u64 counter = args._counter == COUNTER_SAMPLES ? (*it)->samples : (*it)->counter;
        if (counter == 0) continue;

        for (int j = trace->num_frames - 1; j >= 0; j--) {
            const char* frame_name = fn.name(trace->frames[j]);
            out << frame_name << (j == 0 ? ' ' : ';');
        }
        // Beware of locale-sensitive conversion
        out.write(buf, snprintf(buf, sizeof(buf), "%llu\n", counter));
    }

    if (!out.good()) {
        Log::warn("Output file may be incomplete");
    }
}

void Profiler::dumpFlameGraph(Writer& out, Arguments& args, bool tree) {
    char title[64];
    if (args._title == NULL) {
        Engine* active_engine = activeEngine();
        if (args._counter == COUNTER_SAMPLES) {
            strcpy(title, active_engine->title());
        } else {
            snprintf(title, sizeof(title), "%s (%s)", active_engine->title(), active_engine->units());
        }
    }

    FlameGraph flamegraph(args._title == NULL ? title : args._title, args._counter, args._minwidth, args._reverse);

    {
        FrameName fn(args, args._style & ~STYLE_ANNOTATE, _epoch, _thread_names_lock, _thread_names);

        std::vector<CallTraceSample*> samples;
        _call_trace_storage.collectSamples(samples);

        for (std::vector<CallTraceSample*>::const_iterator it = samples.begin(); it != samples.end(); ++it) {
            CallTrace* trace = (*it)->acquireTrace();
            if (trace == NULL || excludeTrace(&fn, trace)) continue;

            u64 counter = args._counter == COUNTER_SAMPLES ? (*it)->samples : (*it)->counter;
            if (counter == 0) continue;

            int num_frames = trace->num_frames;

            Trie* f = flamegraph.root();
            if (args._reverse) {
                // Thread frames always come first
                if (_add_sched_frame) {
                    const char* frame_name = fn.name(trace->frames[--num_frames]);
                    f = flamegraph.addChild(f, frame_name, FRAME_NATIVE, counter);
                }
                if (_add_thread_frame) {
                    const char* frame_name = fn.name(trace->frames[--num_frames]);
                    f = flamegraph.addChild(f, frame_name, FRAME_NATIVE, counter);
                }

                for (int j = 0; j < num_frames; j++) {
                    const char* frame_name = fn.name(trace->frames[j]);
                    FrameTypeId frame_type = fn.type(trace->frames[j]);
                    f = flamegraph.addChild(f, frame_name, frame_type, counter);
                }
            } else {
                for (int j = num_frames - 1; j >= 0; j--) {
                    const char* frame_name = fn.name(trace->frames[j]);
                    FrameTypeId frame_type = fn.type(trace->frames[j]);
                    f = flamegraph.addChild(f, frame_name, frame_type, counter);
                }
            }
            f->_total += counter;
            f->_self += counter;
        }
    }

    flamegraph.dump(out, tree);
}

void Profiler::dumpText(Writer& out, Arguments& args) {
    FrameName fn(args, args._style | STYLE_DOTTED, _epoch, _thread_names_lock, _thread_names);
    char buf[1024] = {0};

    std::vector<CallTraceSample> samples;
    u64 total_counter = 0;
    {
        std::map<u64, CallTraceSample> map;
        _call_trace_storage.collectSamples(map);
        samples.reserve(map.size());

        for (std::map<u64, CallTraceSample>::const_iterator it = map.begin(); it != map.end(); ++it) {
            CallTrace* trace = it->second.trace;
            u64 counter = it->second.counter;
            if (trace == NULL || counter == 0) continue;

            total_counter += counter;
            if (trace->num_frames == 0 || excludeTrace(&fn, trace)) continue;
            samples.push_back(it->second);
        }
    }

    // Print summary
    snprintf(buf, sizeof(buf) - 1,
            "--- Execution profile ---\n"
            "Total samples       : %lld\n",
            _total_samples);
    out << buf;

    double spercent = 100.0 / _total_samples;
    for (int i = 1; i < ASGCT_FAILURE_TYPES; i++) {
        const char* err_string = asgctError(-i);
        if (err_string != NULL && _failures[i] > 0) {
            snprintf(buf, sizeof(buf), "%-20s: %lld (%.2f%%)\n", err_string, _failures[i], _failures[i] * spercent);
            out << buf;
        }
    }
    out << "\n";

    double cpercent = 100.0 / total_counter;
    const char* units_str = activeEngine()->units();

    // Print top call stacks
    if (args._dump_traces > 0) {
        std::sort(samples.begin(), samples.end());

        int max_count = args._dump_traces;
        for (std::vector<CallTraceSample>::const_iterator it = samples.begin(); it != samples.end() && --max_count >= 0; ++it) {
            snprintf(buf, sizeof(buf) - 1, "--- %lld %s (%.2f%%), %lld sample%s\n",
                     it->counter, units_str, it->counter * cpercent,
                     it->samples, it->samples == 1 ? "" : "s");
            out << buf;

            CallTrace* trace = it->trace;
            for (int j = 0; j < trace->num_frames; j++) {
                const char* frame_name = fn.name(trace->frames[j]);
                snprintf(buf, sizeof(buf) - 1, "  [%2d] %s\n", j, frame_name);
                out << buf;
            }
            out << "\n";
        }
    }

    // Print top methods
    if (args._dump_flat > 0) {
        std::map<std::string, MethodSample> histogram;
        for (std::vector<CallTraceSample>::const_iterator it = samples.begin(); it != samples.end(); ++it) {
            const char* frame_name = fn.name(it->trace->frames[0]);
            histogram[frame_name].add(it->samples, it->counter);
        }

        std::vector<NamedMethodSample> methods(histogram.begin(), histogram.end());
        std::sort(methods.begin(), methods.end(), sortByCounter);

        snprintf(buf, sizeof(buf) - 1, "%12s  percent  samples  top\n"
                                       "  ----------  -------  -------  ---\n", units_str);
        out << buf;

        int max_count = args._dump_flat;
        for (std::vector<NamedMethodSample>::const_iterator it = methods.begin(); it != methods.end() && --max_count >= 0; ++it) {
            snprintf(buf, sizeof(buf) - 1, "%12lld  %6.2f%%  %7lld  %s\n",
                     it->second.counter, it->second.counter * cpercent, it->second.samples, it->first.c_str());
            out << buf;
        }
    }
}

time_t Profiler::addTimeout(time_t start, int timeout) {
    if (timeout == 0) {
        return (time_t)0x7fffffff;
    } else if (timeout > 0) {
        return start + timeout;
    }

    struct tm t;
    localtime_r(&start, &t);

    int hh = (timeout >> 16) & 0xff;
    if (hh < 24) {
        t.tm_hour = hh;
    }
    int mm = (timeout >> 8) & 0xff;
    if (mm < 60) {
        t.tm_min = mm;
    }
    int ss = timeout & 0xff;
    if (ss < 60) {
        t.tm_sec = ss;
    }

    time_t result = mktime(&t);
    if (result <= start) {
        result += (hh < 24 ? 86400 : (mm < 60 ? 3600 : 60));
    }
    return result;
}

void Profiler::startTimer() {
    if (VM::loaded()) {
        JNIEnv* jni = VM::jni();
        jclass Thread = jni->FindClass("java/lang/Thread");
        jmethodID init = jni->GetMethodID(Thread, "<init>", "(Ljava/lang/String;)V");
        jmethodID setDaemon = jni->GetMethodID(Thread, "setDaemon", "(Z)V");

        jstring name = jni->NewStringUTF("Async-profiler Timer");
        if (name != NULL && init != NULL && setDaemon != NULL) {
            jthread thread_obj = jni->NewObject(Thread, init, name);
            if (thread_obj != NULL) {
                jni->CallVoidMethod(thread_obj, setDaemon, JNI_TRUE);
                MutexLocker ml(_timer_lock);
                _timer_id = (void*)(intptr_t)(0x80000000 | _epoch);
                if (VM::jvmti()->RunAgentThread(thread_obj, jvmtiTimerEntry, _timer_id, JVMTI_THREAD_NORM_PRIORITY) == 0) {
                    return;
                }
                _timer_id = NULL;
            }
        }

        jni->ExceptionDescribe();
    } else {
        // If profiling a native app, start a raw pthread instead of a JVM thread
        MutexLocker ml(_timer_lock);
        _timer_id = (void*)(intptr_t)(0x80000000 | _epoch);
        pthread_t thread;
        if (pthread_create(&thread, NULL, pthreadTimerEntry, _timer_id) == 0) {
            pthread_detach(thread);
            return;
        }
        _timer_id = NULL;
    }
}

void Profiler::stopTimer() {
    MutexLocker ml(_timer_lock);
    if (_timer_id != NULL) {
        _timer_id = NULL;
        _timer_lock.notify();
    }
}

void Profiler::timerLoop(void* timer_id) {
    u64 current_micros = OS::micros();
    u64 stop_micros = _stop_time * 1000000ULL;
    u64 sleep_until = _jfr.active() ? current_micros + 1000000 : stop_micros;

    while (true) {
        {
            // Release _timer_lock after sleep to avoid deadlock with Profiler::stop
            MutexLocker ml(_timer_lock);
            while (_timer_id == timer_id && !_timer_lock.waitUntil(sleep_until)) {
                // timeout not reached
            }
            if (_timer_id != timer_id) return;
        }

        if ((current_micros = OS::micros()) >= stop_micros) {
            restart(_global_args);
            return;
        }

        bool need_switch_chunk = _jfr.timerTick(current_micros, _gc_id);
        if (need_switch_chunk) {
            // Flush under profiler state lock
            flushJfr();
        }

        sleep_until = current_micros + 1000000;
    }
}

Error Profiler::runInternal(Arguments& args, Writer& out) {
    switch (args._action) {
        case ACTION_START:
        case ACTION_RESUME: {
            Error error = start(args, args._action == ACTION_START);
            if (error) {
                return error;
            }
            out << "Profiling started\n";
            break;
        }
        case ACTION_STOP: {
            Error error = stop();
            if (args._output == OUTPUT_NONE) {
                if (error) {
                    return error;
                }
                out << "Profiling stopped after " << uptime() << " seconds. No dump options specified\n";
                break;
            }
            // Fall through
        }
        case ACTION_DUMP: {
            Error error = dump(out, args);
            if (error) {
                return error;
            }
            break;
        }
        case ACTION_CHECK: {
            Error error = check(args);
            if (error) {
                return error;
            }
            out << "OK\n";
            break;
        }
        case ACTION_STATUS: {
            MutexLocker ml(_state_lock);
            if (_state == RUNNING) {
                out << "Profiling is running for " << uptime() << " seconds\n";
            } else {
                out << "Profiler is not active\n";
            }
            break;
        }
        case ACTION_MEMINFO: {
            MutexLocker ml(_state_lock);
            printUsedMemory(out);
            break;
        }
        case ACTION_LIST: {
            out << "Basic events:\n";
            out << "  " << EVENT_CPU << "\n";
            out << "  " << EVENT_ALLOC << "\n";
            out << "  " << EVENT_LOCK << "\n";
            out << "  " << EVENT_WALL << "\n";
            out << "  " << EVENT_ITIMER << "\n";
            if (CTimer::supported()) {
                out << "  " << EVENT_CTIMER << "\n";
            }

            out << "Java method calls:\n";
            out << "  ClassName.methodName\n";

            if (PerfEvents::supported()) {
                out << "Perf events:\n";
                // The first perf event is "cpu" which is already printed
                for (int event_id = 1; ; event_id++) {
                    const char* event_name = PerfEvents::getEventName(event_id);
                    if (event_name == NULL) break;
                    out << "  " << event_name << "\n";
                }
            }
            break;
        }
        case ACTION_VERSION:
            out << PROFILER_VERSION;
            break;
        default:
            break;
    }
    return Error::OK;
}

Error Profiler::run(Arguments& args) {
    if (!args.hasOutputFile()) {
        LogWriter out;
        return runInternal(args, out);
    } else {
        // Open output file under the lock to avoid races with background timer
        MutexLocker ml(_state_lock);
        FileWriter out(args.file());
        if (!out.is_open()) {
            return Error("Could not open output file");
        }
        return runInternal(args, out);
    }
}

Error Profiler::restart(Arguments& args) {
    MutexLocker ml(_state_lock);

    Error error = stop(args._loop);
    if (error) {
        return error;
    }

    if (args._file != NULL && args._output != OUTPUT_NONE && args._output != OUTPUT_JFR) {
        FileWriter out(args.file());
        if (!out.is_open()) {
            return Error("Could not open output file");
        }
        error = dump(out, args);
        if (error) {
            return error;
        }
    }

    if (args._loop) {
        args._fdtransfer = false;  // keep the previous connection
        args._file_num++;
        return start(args, true);
    }

    return Error::OK;
}

void Profiler::shutdown(Arguments& args) {
    MutexLocker ml(_state_lock);

    // The last chance to dump profile before VM terminates
    if (_state == RUNNING) {
        args._action = ACTION_STOP;
        Error error = run(args);
        if (error) {
            Log::error("%s", error.message());
        }
    }

    _state = TERMINATED;
}
