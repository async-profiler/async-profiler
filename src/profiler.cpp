/*
 * Copyright 2016 Andrei Pangin
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

#include <algorithm>
#include <fstream>
#include <dlfcn.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "profiler.h"
#include "perfEvents.h"
#include "allocTracer.h"
#include "lockTracer.h"
#include "wallClock.h"
#include "instrument.h"
#include "itimer.h"
#include "flameGraph.h"
#include "flightRecorder.h"
#include "frameName.h"
#include "log.h"
#include "os.h"
#include "stackFrame.h"
#include "symbols.h"
#include "vmStructs.h"


Profiler Profiler::_instance;

static Engine noop_engine;
static PerfEvents perf_events;
static AllocTracer alloc_tracer;
static LockTracer lock_tracer;
static WallClock wall_clock;
static ITimer itimer;
static Instrument instrument;


// Stack recovery techniques used to workaround AsyncGetCallTrace flaws.
// Can be disabled with 'safemode' option.
enum StackRecovery {
    MOVE_SP      = 0x1,
    MOVE_SP2     = 0x2,
    POP_FRAME    = 0x4,
    SCAN_STACK   = 0x8,
    LAST_JAVA_PC = 0x10,
    GC_TRACES    = 0x20,
    JAVA_STATE   = 0x40,
    MAX_RECOVERY = 0x7f
};


enum EventMask {
    EM_CPU   = 1,
    EM_ALLOC = 2,
    EM_LOCK  = 4
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


void Profiler::addJavaMethod(const void* address, int length, jmethodID method) {
    _jit_lock.lock();
    _java_methods.add(address, length, method, true);
    _jit_lock.unlock();
}

void Profiler::removeJavaMethod(const void* address, jmethodID method) {
    _jit_lock.lock();
    _java_methods.remove(address, method);
    _jit_lock.unlock();
}

void Profiler::resetJavaMethods() {
    _jit_lock.lock();
    _java_methods.reset();
    _jit_lock.unlock();
}

void Profiler::addRuntimeStub(const void* address, int length, const char* name) {
    _stubs_lock.lock();
    _runtime_stubs.add(address, length, name, true);
    _stubs_lock.unlock();
}

void Profiler::onThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    int tid = OS::threadId();
    _thread_filter.remove(tid);
    updateThreadName(jvmti, jni, thread);

    if (_engine == &perf_events) {
        PerfEvents::createForThread(tid);
    }
}

void Profiler::onThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    int tid = OS::threadId();
    _thread_filter.remove(tid);
    updateThreadName(jvmti, jni, thread);

    if (_engine == &perf_events) {
        PerfEvents::destroyForThread(tid);
    }
}

const char* Profiler::asgctError(int code) {
    switch (code) {
        case ticks_no_Java_frame:
        case ticks_unknown_not_Java:
        case ticks_not_walkable_not_Java:
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
        case ticks_deopt:
            return "deoptimization";
        case ticks_safepoint:
            return "safepoint";
        case ticks_skipped:
            return "skipped";
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
    Symbols::parseLibraries(_native_libs, _native_lib_count, MAX_NATIVE_LIBS, kernel_symbols);
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
    if (len > 0 && name[len - 1] == '*') {
        for (int i = 0; i < _native_lib_count; i++) {
            const void* address = _native_libs[i]->findSymbolByPrefix(name, len - 1);
            if (address != NULL) {
                return offsetBPaddr(address);
            }
        }
    } else {
        for (int i = 0; i < _native_lib_count; i++) {
            const void* address = _native_libs[i]->findSymbol(name);
            if (address != NULL) {
                return offsetBPaddr(address);
            }
        }
    }

    return NULL;
}

NativeCodeCache* Profiler::findNativeLibrary(const void* address) {
    const int native_lib_count = _native_lib_count;
    for (int i = 0; i < native_lib_count; i++) {
        if (_native_libs[i]->contains(address)) {
            return _native_libs[i];
        }
    }
    return NULL;
}

const char* Profiler::findNativeMethod(const void* address) {
    NativeCodeCache* lib = findNativeLibrary(address);
    return lib == NULL ? NULL : lib->binarySearch(address);
}

// Make sure the top frame is Java, otherwise AsyncGetCallTrace
// will attempt to use frame pointer based stack walking
bool Profiler::inJavaCode(void* ucontext) {
    if (ucontext == NULL) {
        return true;
    }

    const void* pc = (const void*)StackFrame(ucontext).pc();
    if (_runtime_stubs.contains(pc)) {
        _stubs_lock.lockShared();
        jmethodID method = _runtime_stubs.find(pc);
        _stubs_lock.unlockShared();
        return method == NULL || strcmp((const char*)method, "call_stub") != 0;
    }
    return _java_methods.contains(pc);
}

int Profiler::getNativeTrace(Engine* engine, void* ucontext, ASGCT_CallFrame* frames, int tid) {
    const void* native_callchain[MAX_NATIVE_FRAMES];
    int native_frames = engine->getNativeTrace(ucontext, tid, native_callchain, MAX_NATIVE_FRAMES,
                                               &_java_methods, &_runtime_stubs);

    int depth = 0;
    jmethodID prev_method = NULL;

    for (int i = 0; i < native_frames; i++) {
        jmethodID current_method = (jmethodID)findNativeMethod(native_callchain[i]);
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

int Profiler::getJavaTraceAsync(void* ucontext, ASGCT_CallFrame* frames, int max_depth) {
    VMThread* vm_thread = VMThread::current();
    if (vm_thread == NULL) {
        return 0;
    }

    JNIEnv* jni = VM::jni();
    if (jni == NULL) {
        // Not a Java thread
        return 0;
    }

    if (_safe_mode & JAVA_STATE) {
        int state = vm_thread->state();
        if ((state == 8 || state == 9) && !inJavaCode(ucontext)) {
            // Thread is in Java state, but does not have a valid Java frame on top of the stack
            atomicInc(_failures[-ticks_unknown_Java]);
            frames->bci = BCI_ERROR;
            frames->method_id = (jmethodID)asgctError(ticks_unknown_Java);
            return 1;
        }
    }

    ASGCT_CallTrace trace = {jni, 0, frames};
    VM::_asyncGetCallTrace(&trace, max_depth, ucontext);

    if (trace.num_frames > 0) {
        return trace.num_frames;
    }

    if ((trace.num_frames == ticks_unknown_Java || trace.num_frames == ticks_not_walkable_Java) && _safe_mode < MAX_RECOVERY) {
        // If current Java stack is not walkable (e.g. the top frame is not fully constructed),
        // try to manually pop the top frame off, hoping that the previous frame is walkable.
        // This is a temporary workaround for AsyncGetCallTrace issues,
        // see https://bugs.openjdk.java.net/browse/JDK-8178287
        StackFrame top_frame(ucontext);
        uintptr_t pc = top_frame.pc(),
                  sp = top_frame.sp(),
                  fp = top_frame.fp();

        // Stack might not be walkable if some temporary values are pushed onto the stack
        // above the expected frame SP
        if (!(_safe_mode & MOVE_SP) && CAN_MOVE_SP) {
            for (int extra_stack_slots = 1; extra_stack_slots <= 2; extra_stack_slots++) {
                top_frame.sp() = sp + extra_stack_slots * sizeof(uintptr_t);
                VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
                top_frame.sp() = sp;

                if (trace.num_frames > 0) {
                    return trace.num_frames;
                }
            }
        }

        // Guess top method by PC and insert it manually into the call trace
        bool is_entry_frame = false;
        if (fillTopFrame((const void*)pc, trace.frames)) {
            bool is_native_frame = trace.frames->bci == BCI_NATIVE_FRAME;
            is_entry_frame = is_native_frame && strcmp((const char*)trace.frames->method_id, "call_stub") == 0;

            if (!is_native_frame || _cstack != CSTACK_NO) {
                trace.frames++;
                max_depth--;
            }
        }

        // Attempt further manipulations with top frame, only if SP points to the current stack
        if (top_frame.validSP()) {
            // Retry with the fixed context, but only if PC looks reasonable,
            // otherwise AsyncGetCallTrace may crash
            if (!(_safe_mode & POP_FRAME) && top_frame.pop(is_entry_frame)) {
                if (getAddressType((instruction_t*)top_frame.pc()) != ADDR_UNKNOWN) {
                    VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
                }
                top_frame.restore(pc, sp, fp);

                if (trace.num_frames > 0) {
                    return trace.num_frames + (trace.frames - frames);
                }
            }

            // Retry moving stack pointer, but now in wider range: 3 to 6 slots.
            // Helps to recover from String.indexOf() intrinsic
            if (!(_safe_mode & MOVE_SP2) && CAN_MOVE_SP) {
                ASGCT_CallFrame* prev_frames = trace.frames;
                trace.frames = frames;
                for (int extra_stack_slots = 3; extra_stack_slots <= 6; extra_stack_slots = (extra_stack_slots - 1) << 1) {
                    top_frame.sp() = sp + extra_stack_slots * sizeof(uintptr_t);
                    VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
                    top_frame.sp() = sp;

                    if (trace.num_frames > 0) {
                        return trace.num_frames;
                    }
                }
                trace.frames = prev_frames;
            }

            // Try to find the previous frame by looking a few top stack slots
            // for something that resembles a return address
            if (!(_safe_mode & SCAN_STACK)) {
                for (int slot = 0; slot < StackFrame::callerLookupSlots(); slot++) {
                    if (getAddressType((instruction_t*)top_frame.stackAt(slot)) != ADDR_UNKNOWN) {
                        top_frame.pc() = top_frame.stackAt(slot);
                        top_frame.sp() = sp + (slot + 1) * sizeof(uintptr_t);
                        VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
                        top_frame.restore(pc, sp, fp);

                        if (trace.num_frames > 0) {
                            return trace.num_frames + (trace.frames - frames);
                        }
                    }
                }
            }
        }
    } else if (trace.num_frames == ticks_unknown_not_Java && !(_safe_mode & LAST_JAVA_PC)) {
        uintptr_t& sp = vm_thread->lastJavaSP();
        uintptr_t& pc = vm_thread->lastJavaPC();
        if (sp != 0 && pc == 0) {
            // We have the last Java frame anchor, but it is not marked as walkable.
            // Make it walkable here
            uintptr_t saved_sp = sp;
            pc = ((uintptr_t*)saved_sp)[-1];

            AddressType addr_type = getAddressType((instruction_t*)pc);
            if (addr_type != ADDR_UNKNOWN) {
                // AGCT fails if the last Java frame is a Runtime Stub with an invalid _frame_complete_offset.
                // In this case we manually replace last Java frame to the previous frame
                if (addr_type == ADDR_STUB) {
                    RuntimeStub* stub = RuntimeStub::findBlob((instruction_t*)pc);
                    if (stub != NULL && stub->frameSize() > 0 && stub->frameSize() < 256) {
                        sp = saved_sp + stub->frameSize() * sizeof(uintptr_t);
                        pc = ((uintptr_t*)sp)[-1];
                    }
                }
                VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
            }

            sp = saved_sp;
            pc = 0;
        }
    } else if (trace.num_frames == ticks_not_walkable_not_Java && !(_safe_mode & LAST_JAVA_PC)) {
        uintptr_t& sp = vm_thread->lastJavaSP();
        uintptr_t& pc = vm_thread->lastJavaPC();
        if (sp != 0 && pc != 0 && getAddressType((instruction_t*)pc) == ADDR_STUB) {
            // Similar to the above: last Java frame is set,
            // but points to a Runtime Stub with an invalid _frame_complete_offset
            RuntimeStub* stub = RuntimeStub::findBlob((instruction_t*)pc);
            if (stub != NULL && stub->frameSize() > 0 && stub->frameSize() < 256) {
                uintptr_t saved_sp = sp;
                uintptr_t saved_pc = pc;

                sp = saved_sp + stub->frameSize() * sizeof(uintptr_t);
                pc = ((uintptr_t*)sp)[-1];
                VM::_asyncGetCallTrace(&trace, max_depth, ucontext);

                sp = saved_sp;
                pc = saved_pc;
            }
        }
    } else if (trace.num_frames == ticks_GC_active && !(_safe_mode & GC_TRACES)) {
        if (VMStructs::_get_stack_trace != NULL && CollectedHeap::isGCActive() && !VM::inRedefineClasses()) {
            // While GC is running Java threads are known to be at safepoint
            return getJavaTraceJvmti((jvmtiFrameInfo*)frames, frames, max_depth);
        }
    }

    if (trace.num_frames > 0) {
        return trace.num_frames;
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

int Profiler::getJavaTraceJvmti(jvmtiFrameInfo* jvmti_frames, ASGCT_CallFrame* frames, int max_depth) {
    // We cannot call pure JVM TI here, because it assumes _thread_in_native state,
    // but allocation events happen in _thread_in_vm state,
    // see https://github.com/jvm-profiling-tools/async-profiler/issues/64
    JNIEnv* jni = VM::jni();
    if (jni == NULL) {
        return 0;
    }

    VMThread* vm_thread = VMThread::fromEnv(jni);
    int num_frames;
    if (VMStructs::_get_stack_trace(NULL, vm_thread, 0, max_depth, jvmti_frames, &num_frames) == 0 && num_frames > 0) {
        // Profiler expects stack trace in AsyncGetCallTrace format; convert it now
        for (int i = 0; i < num_frames; i++) {
            frames[i].method_id = jvmti_frames[i].method;
            frames[i].bci = 0;
        }
        return num_frames;
    }

    return 0;
}

inline int Profiler::makeEventFrame(ASGCT_CallFrame* frames, jint event_type, uintptr_t id) {
    frames[0].bci = event_type;
    frames[0].method_id = (jmethodID)id;
    return 1;
}

bool Profiler::fillTopFrame(const void* pc, ASGCT_CallFrame* frame) {
    jmethodID method = NULL;

    // Check if PC belongs to a JIT compiled method
    _jit_lock.lockShared();
    if (_java_methods.contains(pc) && (method = _java_methods.find(pc)) != NULL) {
        frame->bci = 0;
        frame->method_id = method;
    }
    _jit_lock.unlockShared();

    if (method != NULL) {
        return true;
    }

    // Check if PC belongs to a VM runtime stub
    _stubs_lock.lockShared();
    if (_runtime_stubs.contains(pc) && (method = _runtime_stubs.find(pc)) != NULL) {
        frame->bci = BCI_NATIVE_FRAME;
        frame->method_id = method;
    }
    _stubs_lock.unlockShared();

    return method != NULL;
}

AddressType Profiler::getAddressType(instruction_t* pc) {
    bool in_generated_code = false;

    // 1. Check if PC lies within JVM's compiled code cache
    if (_java_methods.contains(pc)) {
        _jit_lock.lockShared();
        jmethodID method = _java_methods.find(pc);
        _jit_lock.unlockShared();
        if (method != NULL) {
            return ADDR_JIT;
        }
        in_generated_code = true;
    }

    // 2. The same for VM runtime stubs
    if (_runtime_stubs.contains(pc)) {
        _stubs_lock.lockShared();
        jmethodID method = _runtime_stubs.find(pc);
        _stubs_lock.unlockShared();
        if (method != NULL) {
            return ADDR_STUB;
        }
        in_generated_code = true;
    }

    // 3. Check if PC belongs to executable code of shared libraries
    if (!in_generated_code) {
        const int native_lib_count = _native_lib_count;
        for (int i = 0; i < native_lib_count; i++) {
            if (_native_libs[i]->contains(pc)) {
                return ADDR_NATIVE;
            }
        }
    }

    // This can be some other dynamically generated code, but we don't know it. Better stay safe.
    return ADDR_UNKNOWN;
}

void Profiler::recordSample(void* ucontext, u64 counter, jint event_type, Event* event) {
    atomicInc(_total_samples);

    int tid = OS::threadId();
    u32 lock_index = getLockIndex(tid);
    if (!_locks[lock_index].tryLock() &&
        !_locks[lock_index = (lock_index + 1) % CONCURRENCY_LEVEL].tryLock() &&
        !_locks[lock_index = (lock_index + 2) % CONCURRENCY_LEVEL].tryLock())
    {
        // Too many concurrent signals already
        atomicInc(_failures[-ticks_skipped]);

        if (event_type == 0 && _engine == &perf_events) {
            // Need to reset PerfEvents ring buffer, even though we discard the collected trace
            PerfEvents::resetBuffer(tid);
        }
        return;
    }

    ASGCT_CallFrame* frames = _calltrace_buffer[lock_index]->_asgct_frames;

    int num_frames = 0;
    if (!_jfr.active() && event_type <= BCI_ALLOC && event_type >= BCI_PARK && event->id()) {
        num_frames = makeEventFrame(frames, event_type, event->id());
    }

    // Use engine stack walker for execution samples, or basic stack walker for other events
    if (event_type == 0 && _cstack != CSTACK_NO) {
        num_frames += getNativeTrace(_engine, ucontext, frames + num_frames, tid);
    } else if (event_type != 0 && _cstack > CSTACK_NO) {
        num_frames += getNativeTrace(&noop_engine, ucontext, frames + num_frames, tid);
    }

    int first_java_frame = num_frames;
    if (event_type != 0 && VMStructs::_get_stack_trace != NULL) {
        // Events like object allocation happen at known places where it is safe to call JVM TI
        jvmtiFrameInfo* jvmti_frames = _calltrace_buffer[lock_index]->_jvmti_frames;
        num_frames += getJavaTraceJvmti(jvmti_frames + num_frames, frames + num_frames, _max_stack_depth);
    } else {
        num_frames += getJavaTraceAsync(ucontext, frames + num_frames, _max_stack_depth);
    }

    if (num_frames == 0) {
        num_frames += makeEventFrame(frames + num_frames, BCI_ERROR, (uintptr_t)"no_Java_frame");
    } else if (event_type == BCI_INSTRUMENT) {
        // Skip Instrument.recordSample() method
        frames++;
        num_frames--;
    }

    // TODO: Zero out top bci to reduce the number of stack traces
    // if (first_java_frame < num_frames && frames[first_java_frame].bci > 0) {
    //     frames[first_java_frame].bci = 0;
    // }

    if (_add_thread_frame) {
        num_frames += makeEventFrame(frames + num_frames, BCI_THREAD_ID, tid);
    }

    u32 call_trace_id = _call_trace_storage.put(num_frames, frames, counter);
    _jfr.recordEvent(lock_index, tid, call_trace_id, event_type, event, counter);

    _locks[lock_index].unlock();
}

jboolean JNICALL Profiler::NativeLibraryLoadTrap(JNIEnv* env, jobject self, jstring name, jboolean builtin) {
    jboolean result = ((jboolean JNICALL (*)(JNIEnv*, jobject, jstring, jboolean))
                       _instance._original_NativeLibrary_load)(env, self, name, builtin);
    _instance.updateSymbols(false);
    return result;
}

jboolean JNICALL Profiler::NativeLibrariesLoadTrap(JNIEnv* env, jobject self, jobject lib, jstring name, jboolean builtin, jboolean jni) {
    jboolean result = ((jboolean JNICALL (*)(JNIEnv*, jobject, jobject, jstring, jboolean, jboolean))
                       _instance._original_NativeLibrary_load)(env, self, lib, name, builtin, jni);
    _instance.updateSymbols(false);
    return result;
}

void JNICALL Profiler::ThreadSetNativeNameTrap(JNIEnv* env, jobject self, jstring name) {
    ((void JNICALL (*)(JNIEnv*, jobject, jstring))_instance._original_Thread_setNativeName)(env, self, name);
    _instance.updateThreadName(VM::jvmti(), env, self);
}

void Profiler::bindNativeLibraryLoad(JNIEnv* env, bool enable) {
    jclass NativeLibrary;

    if (_original_NativeLibrary_load == NULL) {
        char original_jni_name[64];

        if ((NativeLibrary = env->FindClass("jdk/internal/loader/NativeLibraries")) != NULL) {
            strcpy(original_jni_name, "Java_jdk_internal_loader_NativeLibraries_");
            _trapped_NativeLibrary_load = (void*)NativeLibrariesLoadTrap;

            // JDK 15+
            _load_method.name = (char*)"load";
            _load_method.signature = (char*)"(Ljdk/internal/loader/NativeLibraries$NativeLibraryImpl;Ljava/lang/String;ZZ)Z";

        } else if ((NativeLibrary = env->FindClass("java/lang/ClassLoader$NativeLibrary")) != NULL) {
            strcpy(original_jni_name, "Java_java_lang_ClassLoader_00024NativeLibrary_");
            _trapped_NativeLibrary_load = (void*)NativeLibraryLoadTrap;

            if (env->GetMethodID(NativeLibrary, "load0", "(Ljava/lang/String;Z)Z") != NULL) {
                // JDK 9-14
                _load_method.name = (char*)"load0";
                _load_method.signature = (char*)"(Ljava/lang/String;Z)Z";
            } else if (env->GetMethodID(NativeLibrary, "load", "(Ljava/lang/String;Z)V") != NULL) {
                // JDK 8
                _load_method.name = (char*)"load";
                _load_method.signature = (char*)"(Ljava/lang/String;Z)V";
            } else {
                // JDK 7
                _load_method.name = (char*)"load";
                _load_method.signature = (char*)"(Ljava/lang/String;)V";
            }

        } else {
            Log::warn("Failed to intercept NativeLibraries.load()");
            return;
        }

        strcat(original_jni_name, _load_method.name);
        if ((_original_NativeLibrary_load = dlsym(VM::_libjava, original_jni_name)) == NULL) {
            Log::warn("Could not find %s", original_jni_name);
            return;
        }

    } else {
        const char* class_name = _trapped_NativeLibrary_load == NativeLibrariesLoadTrap
            ? "jdk/internal/loader/NativeLibraries"
            : "java/lang/ClassLoader$NativeLibrary";
        if ((NativeLibrary = env->FindClass(class_name)) == NULL) {
            Log::warn("Could not find %s", class_name);
            return;
        }
    }

    _load_method.fnPtr = enable ? _trapped_NativeLibrary_load : _original_NativeLibrary_load;
    env->RegisterNatives(NativeLibrary, &_load_method, 1);
}

void Profiler::bindThreadSetNativeName(JNIEnv* env, bool enable) {
    jclass Thread = env->FindClass("java/lang/Thread");
    if (Thread == NULL) {
        return;
    }

    // Find JNI entry for Thread.setNativeName() method
    if (_original_Thread_setNativeName == NULL) {
        _original_Thread_setNativeName = dlsym(VM::_libjvm, "JVM_SetNativeThreadName");
    }

    // Change function pointer for the native method
    if (_original_Thread_setNativeName != NULL) {
        void* entry = enable ? (void*)ThreadSetNativeNameTrap : _original_Thread_setNativeName;
        const JNINativeMethod setNativeName = {(char*)"setNativeName", (char*)"(Ljava/lang/String;)V", entry};
        env->RegisterNatives(Thread, &setNativeName, 1);
    }
}

void Profiler::switchNativeMethodTraps(bool enable) {
    JNIEnv* env = VM::jni();
    bindNativeLibraryLoad(env, enable);
    // bindThreadSetNativeName(env, enable);
    env->ExceptionClear();
}

Error Profiler::installTraps(const char* begin, const char* end) {
    if (begin == NULL) {
        _begin_trap.assign(NULL);
    } else {
        const void* begin_addr = resolveSymbol(begin);
        if (begin_addr == NULL || !_begin_trap.assign(begin_addr)) {
            return Error("Begin address not found");
        }
    }

    if (end == NULL) {
        _end_trap.assign(NULL);
    } else {
        const void* end_addr = resolveSymbol(end);
        if (end_addr == NULL || !_end_trap.assign(end_addr)) {
            return Error("End address not found");
        }
    }

    if (_begin_trap.entry() == 0) {
        _engine->enableEvents(true);
    } else {
        _engine->enableEvents(false);
        _begin_trap.install();
    }

    return Error::OK;
}

void Profiler::uninstallTraps() {
    _begin_trap.uninstall();
    _end_trap.uninstall();
}

void Profiler::trapHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    StackFrame frame(ucontext);

    if (_begin_trap.covers(frame.pc())) {
        _engine->enableEvents(true);
        _begin_trap.uninstall();
        _end_trap.install();
        frame.pc() = _begin_trap.entry();
    } else if (_end_trap.covers(frame.pc())) {
        _engine->enableEvents(false);
        _end_trap.uninstall();
        _begin_trap.install();
        frame.pc() = _end_trap.entry();
    } else if (_orig_trapHandler != NULL) {
        _orig_trapHandler(signo, siginfo, ucontext);
    }
}

void Profiler::setupTrapHandler() {
    _orig_trapHandler = OS::installSignalHandler(SIGTRAP, AllocTracer::trapHandler);
    if (_orig_trapHandler == (void*)SIG_DFL || _orig_trapHandler == (void*)SIG_IGN) {
        _orig_trapHandler = NULL;
    }
}

void Profiler::setThreadInfo(int tid, const char* name, jlong java_thread_id) {
    MutexLocker ml(_thread_names_lock);
    _thread_names[tid] = name;
    _thread_ids[tid] = java_thread_id;
}

void Profiler::updateThreadName(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    if (_update_thread_names && VMThread::hasNativeId()) {
        VMThread* vm_thread = VMThread::fromJavaThread(jni, thread);
        jvmtiThreadInfo thread_info;
        if (vm_thread != NULL && jvmti->GetThreadInfo(thread, &thread_info) == 0) {
            jlong java_thread_id = VMThread::javaThreadId(jni, thread);
            setThreadInfo(vm_thread->osThreadId(), thread_info.name, java_thread_id);
            jvmti->Deallocate((unsigned char*)thread_info.name);
        }
    }
}

void Profiler::updateJavaThreadNames() {
    if (_update_thread_names && VMThread::hasNativeId()) {
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
        return &wall_clock;
    } else if (strcmp(event_name, EVENT_ITIMER) == 0) {
        return &itimer;
    } else if (strchr(event_name, '.') != NULL && strchr(event_name, ':') == NULL) {
        return &instrument;
    } else {
        return &perf_events;
    }
}

Engine* Profiler::activeEngine() {
    switch (_event_mask) {
        case EM_ALLOC:
            return &alloc_tracer;
        case EM_LOCK:
            return &lock_tracer;
        default:
            return _engine;
    }
}

Error Profiler::checkJvmCapabilities() {
    if (VMStructs::libjvm() == NULL) {
        return Error("Could not find libjvm among loaded libraries. Unsupported JVM?");
    }

    if (!VMStructs::hasThreadBridge()) {
        return Error("Could not find VMThread bridge. Unsupported JVM?");
    }

    if (VMStructs::_get_stack_trace == NULL) {
        Log::warn("Install JVM debug symbols to improve profile accuracy");
    }

    return Error::OK;
}

Error Profiler::start(Arguments& args, bool reset) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) {
        return Error("Profiler already started");
    }

    Error error = checkJvmCapabilities();
    if (error) {
        return error;
    }

    _event_mask = (args._event != NULL ? EM_CPU : 0) |
                  (args._alloc > 0 ? EM_ALLOC : 0) |
                  (args._lock > 0 ? EM_LOCK : 0);
    if (_event_mask == 0) {
        return Error("No profiling events specified");
    } else if ((_event_mask & (_event_mask - 1)) && args._output != OUTPUT_JFR) {
        return Error("Only JFR output supports multiple events");
    }

    if (reset || _start_time == 0) {
        // Reset counters
        _total_samples = 0;
        memset(_failures, 0, sizeof(_failures));

        // Reset dicrionaries and bitmaps
        _class_map.clear();
        _thread_filter.clear();
        _call_trace_storage.clear();

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

    updateSymbols(args._ring != RING_USER);

    _safe_mode = args._safe_mode;
    if (VM::hotspot_version() < 8) {
        // Cannot use JVM TI stack walker during GC on non-HotSpot JVMs or with PermGen
        _safe_mode |= GC_TRACES | LAST_JAVA_PC;
    }

    _add_thread_frame = args._threads && args._output != OUTPUT_JFR;
    _update_thread_names = args._threads || args._output == OUTPUT_JFR;
    _thread_filter.init(args._filter);

    _engine = selectEngine(args._event);
    _cstack = args._cstack;
    if (_cstack == CSTACK_LBR && _engine != &perf_events) {
        return Error("Branch stack is supported only with PMU events");
    }

    error = installTraps(args._begin, args._end);
    if (error) {
        return error;
    }

    if (args._output == OUTPUT_JFR) {
        error = _jfr.start(args, reset);
        if (error) {
            uninstallTraps();
            return error;
        }
    }

    error = _engine->start(args);
    if (error) {
        goto error1;
    }

    if (_event_mask & EM_ALLOC) {
        error = alloc_tracer.start(args);
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

    // Thread events might be already enabled by PerfEvents::start
    switchThreadEvents(JVMTI_ENABLE);
    switchNativeMethodTraps(true);

    _state = RUNNING;
    _start_time = time(NULL);
    return Error::OK;

error3:
    if (_event_mask & EM_ALLOC) alloc_tracer.stop();

error2:
    _engine->stop();

error1:
    uninstallTraps();
    for (int i = 0; i < CONCURRENCY_LEVEL; i++) _locks[i].lock();
    _jfr.stop();
    for (int i = 0; i < CONCURRENCY_LEVEL; i++) _locks[i].unlock();
    return error;
}

Error Profiler::stop() {
    MutexLocker ml(_state_lock);
    if (_state != RUNNING) {
        return Error("Profiler is not active");
    }

    uninstallTraps();

    if (_event_mask & EM_LOCK) lock_tracer.stop();
    if (_event_mask & EM_ALLOC) alloc_tracer.stop();

    _engine->stop();

    switchNativeMethodTraps(false);
    switchThreadEvents(JVMTI_DISABLE);
    updateJavaThreadNames();
    updateNativeThreadNames();

    // Acquire all spinlocks to avoid race with remaining signals
    for (int i = 0; i < CONCURRENCY_LEVEL; i++) _locks[i].lock();
    _jfr.stop();
    for (int i = 0; i < CONCURRENCY_LEVEL; i++) _locks[i].unlock();

    _state = IDLE;
    return Error::OK;
}

Error Profiler::check(Arguments& args) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) {
        return Error("Profiler already started");
    }

    Error error = checkJvmCapabilities();
    if (error) {
        return error;
    }

    _engine = selectEngine(args._event);
    return _engine->check(args);
}

void Profiler::switchThreadEvents(jvmtiEventMode mode) {
    if (_thread_events_state != mode) {
        jvmtiEnv* jvmti = VM::jvmti();
        jvmti->SetEventNotificationMode(mode, JVMTI_EVENT_THREAD_START, NULL);
        jvmti->SetEventNotificationMode(mode, JVMTI_EVENT_THREAD_END, NULL);
        _thread_events_state = mode;
    }
}

void Profiler::dump(std::ostream& out, Arguments& args) {
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
        default:
            break;
    }
}

/*
 * Dump stacks in FlameGraph input format:
 * 
 * <frame>;<frame>;...;<topmost frame> <count>
 */
void Profiler::dumpCollapsed(std::ostream& out, Arguments& args) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE || _engine == NULL) return;

    FrameName fn(args, args._style, _thread_names_lock, _thread_names);

    std::vector<CallTraceSample*> samples;
    _call_trace_storage.collectSamples(samples);

    for (std::vector<CallTraceSample*>::const_iterator it = samples.begin(); it != samples.end(); ++it) {
        CallTrace* trace = (*it)->trace;
        if (excludeTrace(&fn, trace)) continue;

        for (int j = trace->num_frames - 1; j >= 0; j--) {
            const char* frame_name = fn.name(trace->frames[j]);
            out << frame_name << (j == 0 ? ' ' : ';');
        }
        out << (args._counter == COUNTER_SAMPLES ? (*it)->samples : (*it)->counter) << "\n";
    }
}

void Profiler::dumpFlameGraph(std::ostream& out, Arguments& args, bool tree) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE || _engine == NULL) return;

    char title[64];
    if (args._title == NULL) {
        Engine* active_engine = activeEngine();
        if (args._counter == COUNTER_SAMPLES) {
            strcpy(title, active_engine->title());
        } else {
            sprintf(title, "%s (%s)", active_engine->title(), active_engine->units());
        }
    }

    FlameGraph flamegraph(args._title == NULL ? title : args._title, args._counter, args._minwidth, args._reverse);
    FrameName fn(args, args._style, _thread_names_lock, _thread_names);

    std::vector<CallTraceSample*> samples;
    _call_trace_storage.collectSamples(samples);

    for (std::vector<CallTraceSample*>::const_iterator it = samples.begin(); it != samples.end(); ++it) {
        CallTrace* trace = (*it)->trace;
        if (excludeTrace(&fn, trace)) continue;

        u64 samples = (args._counter == COUNTER_SAMPLES ? (*it)->samples : (*it)->counter);
        int num_frames = trace->num_frames;

        Trie* f = flamegraph.root();
        if (args._reverse) {
            if (_add_thread_frame) {
                // Thread frames always come first
                num_frames--;
                const char* frame_name = fn.name(trace->frames[num_frames]);
                f = f->addChild(frame_name, samples);
            }

            for (int j = 0; j < num_frames; j++) {
                const char* frame_name = fn.name(trace->frames[j]);
                f = f->addChild(frame_name, samples);
            }
        } else {
            for (int j = num_frames - 1; j >= 0; j--) {
                const char* frame_name = fn.name(trace->frames[j]);
                f = f->addChild(frame_name, samples);
            }
        }
        f->addLeaf(samples);
    }

    flamegraph.dump(out, tree);
}

void Profiler::dumpText(std::ostream& out, Arguments& args) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE || _engine == NULL) return;

    FrameName fn(args, args._style | STYLE_DOTTED, _thread_names_lock, _thread_names);
    char buf[1024] = {0};

    std::vector<CallTraceSample> samples;
    u64 total_counter = 0;
    {
        std::map<u64, CallTraceSample> map;
        _call_trace_storage.collectSamples(map);
        samples.reserve(map.size());

        for (std::map<u64, CallTraceSample>::const_iterator it = map.begin(); it != map.end(); ++it) {
            total_counter += it->second.counter;
            CallTrace* trace = it->second.trace;
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
    out << std::endl;

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

Error Profiler::runInternal(Arguments& args, std::ostream& out) {
    switch (args._action) {
        case ACTION_START:
        case ACTION_RESUME: {
            Error error = start(args, args._action == ACTION_START);
            if (error) {
                return error;
            }
            out << "Profiling started" << std::endl;
            break;
        }
        case ACTION_STOP: {
            Error error = stop();
            if (error) {
                return error;
            }
            out << "Profiling stopped after " << uptime() << " seconds. No dump options specified" << std::endl;
            break;
        }
        case ACTION_CHECK: {
            Error error = check(args);
            if (error) {
                return error;
            }
            out << "OK" << std::endl;
            break;
        }
        case ACTION_STATUS: {
            MutexLocker ml(_state_lock);
            if (_state == RUNNING) {
                out << "Profiling is running for " << uptime() << " seconds" << std::endl;
            } else {
                out << "Profiler is not active" << std::endl;
            }
            break;
        }
        case ACTION_LIST: {
            out << "Basic events:" << std::endl;
            out << "  " << EVENT_CPU << std::endl;
            out << "  " << EVENT_ALLOC << std::endl;
            out << "  " << EVENT_LOCK << std::endl;
            out << "  " << EVENT_WALL << std::endl;
            out << "  " << EVENT_ITIMER << std::endl;

            out << "Java method calls:" << std::endl;
            out << "  ClassName.methodName" << std::endl;

            if (PerfEvents::supported()) {
                out << "Perf events:" << std::endl;
                // The first perf event is "cpu" which is already printed
                for (int event_id = 1; ; event_id++) {
                    const char* event_name = PerfEvents::getEventName(event_id);
                    if (event_name == NULL) break;
                    out << "  " << event_name << std::endl;
                }
            }
            break;
        }
        case ACTION_VERSION:
            out << PROFILER_VERSION;
            out.flush();
            break;
        case ACTION_FULL_VERSION:
            out << FULL_VERSION_STRING;
            break;
        case ACTION_DUMP:
            stop();
            dump(out, args);
            break;
        default:
            break;
    }
    return Error::OK;
}

Error Profiler::run(Arguments& args) {
    if (!args.hasOutputFile()) {
        return runInternal(args, std::cout);
    } else {
        std::ofstream out(args._file, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            return Error("Could not open output file");
        }
        Error error = runInternal(args, out);
        out.close();
        return error;
    }
}

void Profiler::shutdown(Arguments& args) {
    MutexLocker ml(_state_lock);

    // The last chance to dump profile before VM terminates
    if (_state == RUNNING) {
        args._action = ACTION_DUMP;
        Error error = args._output == OUTPUT_NONE ? stop() : run(args);
        if (error) {
            Log::error(error.message());
        }
    }

    _state = TERMINATED;
}
