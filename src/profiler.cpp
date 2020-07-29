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
#include "os.h"
#include "stackFrame.h"
#include "symbols.h"
#include "vmStructs.h"


Profiler Profiler::_instance;

static PerfEvents perf_events;
static AllocTracer alloc_tracer;
static LockTracer lock_tracer;
static WallClock wall_clock;
static ITimer itimer;
static Instrument instrument;


// Stack recovery techniques used to workaround AsyncGetCallTrace flaws.
// Can be disabled with 'safemode' option.
enum StackRecovery {
    MOVE_SP      = 1,
    POP_FRAME    = 2,
    SCAN_STACK   = 4,
    LAST_JAVA_PC = 8,
    GC_TRACES    = 16,

    HOTSPOT_ONLY = LAST_JAVA_PC | GC_TRACES,
    MAX_RECOVERY = MOVE_SP | POP_FRAME | SCAN_STACK | LAST_JAVA_PC | GC_TRACES
};


u64 Profiler::hashCallTrace(int num_frames, ASGCT_CallFrame* frames) {
    const u64 M = 0xc6a4a7935bd1e995ULL;
    const int R = 47;

    u64 h = num_frames * M;

    for (int i = 0; i < num_frames; i++) {
        u64 k = (u64)frames[i].method_id;
        k *= M;
        k ^= k >> R;
        k *= M;
        h ^= k;
        h *= M;
    }

    h ^= h >> R;
    h *= M;
    h ^= h >> R;

    return h;
}

int Profiler::storeCallTrace(int num_frames, ASGCT_CallFrame* frames, u64 counter) {
    u64 hash = hashCallTrace(num_frames, frames);
    int bucket = (int)(hash % MAX_CALLTRACES);
    int i = bucket;

    while (_hashes[i] != hash) {
        if (_hashes[i] == 0) {
            if (__sync_bool_compare_and_swap(&_hashes[i], 0, hash)) {
                copyToFrameBuffer(num_frames, frames, &_traces[i]);
                break;
            }
            continue;
        }

        if (++i == MAX_CALLTRACES) i = 0;  // move to next slot
        if (i == bucket) return 0;         // the table is full
    }
    
    // CallTrace hash found => atomically increment counter
    atomicInc(_traces[i]._samples);
    atomicInc(_traces[i]._counter, counter);
    return i;
}

void Profiler::copyToFrameBuffer(int num_frames, ASGCT_CallFrame* frames, CallTraceSample* trace) {
    // Atomically reserve space in frame buffer
    int start_frame;
    do {
        start_frame = _frame_buffer_index;
        if (start_frame + num_frames > _frame_buffer_size) {
            _frame_buffer_overflow = true;  // not enough space to store full trace
            return;
        }
    } while (!__sync_bool_compare_and_swap(&_frame_buffer_index, start_frame, start_frame + num_frames));

    trace->_start_frame = start_frame;
    trace->_num_frames = num_frames;

    for (int i = 0; i < num_frames; i++) {
        _frame_buffer[start_frame++] = frames[i];
    }
}

u64 Profiler::hashMethod(jmethodID method) {
    const u64 M = 0xc6a4a7935bd1e995ULL;
    const int R = 17;

    u64 h = (u64)method;

    h ^= h >> R;
    h *= M;
    h ^= h >> R;

    return h;
}

void Profiler::storeMethod(jmethodID method, jint bci, u64 counter) {
    u64 hash = hashMethod(method);
    int bucket = (int)(hash % MAX_CALLTRACES);
    int i = bucket;

    while (_methods[i]._method.method_id != method) {
        if (_methods[i]._method.method_id == NULL) {
            if (__sync_bool_compare_and_swap(&_methods[i]._method.method_id, NULL, method)) {
                _methods[i]._method.bci = bci;
                break;
            }
            continue;
        }
        
        if (++i == MAX_CALLTRACES) i = 0;  // move to next slot
        if (i == bucket) return;           // the table is full
    }

    // Method found => atomically increment counter
    atomicInc(_methods[i]._samples);
    atomicInc(_methods[i]._counter, counter);
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

void Profiler::addRuntimeStub(const void* address, int length, const char* name) {
    _stubs_lock.lock();
    _runtime_stubs.add(address, length, name, true);
    _stubs_lock.unlock();
}

void Profiler::onThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    int tid = OS::threadId();
    _thread_filter.remove(tid);
    updateThreadName(jvmti, jni, thread);
    _engine->onThreadStart(tid);
}

void Profiler::onThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    int tid = OS::threadId();
    _thread_filter.remove(tid);
    updateThreadName(jvmti, jni, thread);
    _engine->onThreadEnd(tid);
}

const char* Profiler::asgctError(int code) {
    switch (code) {
        case ticks_no_Java_frame:
        case ticks_unknown_not_Java:
        case ticks_not_walkable_not_Java:
            // Not in Java context at all; this is not an error
            return NULL;
        case ticks_GC_active:
            return "GC_active";
        case ticks_unknown_Java:
            return "unknown_Java";
        case ticks_not_walkable_Java:
            return "not_walkable_Java";
        case ticks_thread_exit:
            return "thread_exit";
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

void Profiler::updateSymbols() {
    Symbols::parseLibraries(_native_libs, _native_lib_count, MAX_NATIVE_LIBS);
}

const void* Profiler::findSymbol(const char* name) {
    const int native_lib_count = _native_lib_count;
    for (int i = 0; i < native_lib_count; i++) {
        const void* address = _native_libs[i]->findSymbol(name);
        if (address != NULL) {
            return address;
        }
    }
    return NULL;
}

const void* Profiler::findSymbolByPrefix(const char* name) {
    const int native_lib_count = _native_lib_count;
    for (int i = 0; i < native_lib_count; i++) {
        const void* address = _native_libs[i]->findSymbolByPrefix(name);
        if (address != NULL) {
            return address;
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

int Profiler::getNativeTrace(void* ucontext, ASGCT_CallFrame* frames, int tid, bool* stopped_at_java_frame) {
    const void* native_callchain[MAX_NATIVE_FRAMES];
    int native_frames = _engine->getNativeTrace(ucontext, tid, native_callchain, MAX_NATIVE_FRAMES,
                                                &_java_methods, &_runtime_stubs);

    *stopped_at_java_frame = false;
    if (native_frames > 0) {
        const void* last_pc = native_callchain[native_frames - 1];
        if (_java_methods.contains(last_pc) || _runtime_stubs.contains(last_pc)) {
            *stopped_at_java_frame = true;
            native_frames--;
        }
    }

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
    JNIEnv* jni = VM::jni();
    if (jni == NULL) {
        // Not a Java thread
        return 0;
    }

    ASGCT_CallTrace trace = {jni, 0, frames};
    VM::_asyncGetCallTrace(&trace, max_depth, ucontext);

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
        if (!(_safe_mode & MOVE_SP)) {
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
        VMThread* thread = VMThread::fromEnv(jni);
        if (thread != NULL) {
            uintptr_t& sp = thread->lastJavaSP();
            uintptr_t& pc = thread->lastJavaPC();
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
        }
    } else if (trace.num_frames == ticks_GC_active && VMStructs::_get_stack_trace != NULL && !(_safe_mode & GC_TRACES)) {
        // While GC is running Java threads are known to be at safepoint
        return getJavaTraceJvmti((jvmtiFrameInfo*)frames, frames, max_depth);
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

int Profiler::makeEventFrame(ASGCT_CallFrame* frames, jint event_type, jmethodID event) {
    frames[0].bci = event_type;
    frames[0].method_id = event;
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

void Profiler::recordSample(void* ucontext, u64 counter, jint event_type, jmethodID event, ThreadState thread_state) {
    int tid = OS::threadId();

    u64 lock_index = atomicInc(_total_samples) % CONCURRENCY_LEVEL;
    if (!_locks[lock_index].tryLock()) {
        // Too many concurrent signals already
        atomicInc(_failures[-ticks_skipped]);

        if (event_type == 0) {
            // Need to reset PerfEvents ring buffer, even though we discard the collected trace
            _engine->getNativeTrace(ucontext, tid, NULL, 0, &_java_methods, &_runtime_stubs);
        }
        return;
    }

    atomicInc(_total_counter, counter);

    ASGCT_CallFrame* frames = _calltrace_buffer[lock_index]->_asgct_frames;
    bool need_java_trace = true;

    int num_frames = 0;
    if (event != NULL) {
        num_frames = makeEventFrame(frames, event_type, event);
    }
    if (_cstack != CSTACK_NO) {
        num_frames += getNativeTrace(ucontext, frames + num_frames, tid, &need_java_trace);
    }

    if (event_type != 0 && VMStructs::_get_stack_trace != NULL) {
        // Events like object allocation happen at known places where it is safe to call JVM TI
        jvmtiFrameInfo* jvmti_frames = _calltrace_buffer[lock_index]->_jvmti_frames;
        num_frames += getJavaTraceJvmti(jvmti_frames + num_frames, frames + num_frames, _max_stack_depth);
    } else if (OS::isSignalSafeTLS() || need_java_trace) {
        num_frames += getJavaTraceAsync(ucontext, frames + num_frames, _max_stack_depth);
    }

    if (num_frames == 0 || (num_frames == 1 && event != NULL)) {
        num_frames += makeEventFrame(frames + num_frames, BCI_ERROR, (jmethodID)"no_Java_frame");
    } else if (event_type == BCI_INSTRUMENT) {
        // Skip Instrument.recordSample() method
        frames++;
        num_frames--;
    }

    if (_add_thread_frame) {
        num_frames += makeEventFrame(frames + num_frames, BCI_THREAD_ID, (jmethodID)(uintptr_t)tid);
    }

    storeMethod(frames[0].method_id, frames[0].bci, counter);
    int call_trace_id = storeCallTrace(num_frames, frames, counter);
    _jfr.recordExecutionSample(lock_index, tid, call_trace_id, thread_state);

    _locks[lock_index].unlock();
}

jboolean JNICALL Profiler::NativeLibraryLoadTrap(JNIEnv* env, jobject self, jstring name, jboolean builtin) {
    jboolean result = _instance._original_NativeLibrary_load(env, self, name, builtin);
    _instance.updateSymbols();
    return result;
}

void JNICALL Profiler::ThreadSetNativeNameTrap(JNIEnv* env, jobject self, jstring name) {
    _instance._original_Thread_setNativeName(env, self, name);
    _instance.updateThreadName(VM::jvmti(), env, self);
}

void Profiler::bindNativeLibraryLoad(JNIEnv* env, NativeLoadLibraryFunc entry) {
    jclass NativeLibrary = env->FindClass("java/lang/ClassLoader$NativeLibrary");
    if (NativeLibrary == NULL) {
        return;
    }

    // Find JNI entry for NativeLibrary.load() method
    if (_original_NativeLibrary_load == NULL) {
        if (env->GetMethodID(NativeLibrary, "load0", "(Ljava/lang/String;Z)Z") != NULL) {
            // JDK 9+
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

        char jni_name[64];
        strcpy(jni_name, "Java_java_lang_ClassLoader_00024NativeLibrary_");
        strcat(jni_name, _load_method.name);
        _original_NativeLibrary_load = (NativeLoadLibraryFunc)dlsym(VM::_libjava, jni_name);
    }

    // Change function pointer for the native method
    if (_original_NativeLibrary_load != NULL) {
        _load_method.fnPtr = (void*)entry;
        env->RegisterNatives(NativeLibrary, &_load_method, 1);
    }
}

void Profiler::bindThreadSetNativeName(JNIEnv* env, ThreadSetNativeNameFunc entry) {
    jclass Thread = env->FindClass("java/lang/Thread");
    if (Thread == NULL) {
        return;
    }

    // Find JNI entry for Thread.setNativeName() method
    if (_original_Thread_setNativeName == NULL) {
        _original_Thread_setNativeName = (ThreadSetNativeNameFunc)dlsym(VM::_libjvm, "JVM_SetNativeThreadName");
    }

    // Change function pointer for the native method
    if (_original_Thread_setNativeName != NULL) {
        const JNINativeMethod setNativeName = {(char*)"setNativeName", (char*)"(Ljava/lang/String;)V", (void*)entry};
        env->RegisterNatives(Thread, &setNativeName, 1);
    }
}

void Profiler::switchNativeMethodTraps(bool enable) {
    JNIEnv* env = VM::jni();

    if (enable) {
        bindNativeLibraryLoad(env, NativeLibraryLoadTrap);
        // bindThreadSetNativeName(env, ThreadSetNativeNameTrap);
    } else {
        bindNativeLibraryLoad(env, _original_NativeLibrary_load);
        // bindThreadSetNativeName(env, _original_Thread_setNativeName);
    }

    env->ExceptionClear();
}

void Profiler::setThreadInfo(int tid, const char* name, jlong java_thread_id) {
    MutexLocker ml(_thread_names_lock);
    _thread_names[tid] = name;
    _thread_ids[java_thread_id] = tid;
}

void Profiler::updateThreadName(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    if (_update_thread_names) {
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
    if (_update_thread_names) {
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

bool Profiler::excludeTrace(FrameName* fn, CallTraceSample* trace) {
    bool checkInclude = fn->hasIncludeList();
    bool checkExclude = fn->hasExcludeList();
    if (!(checkInclude || checkExclude)) {
        return false;
    }

    for (int i = 0; i < trace->_num_frames; i++) {
        const char* frame_name = fn->name(_frame_buffer[trace->_start_frame + i], true);
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
    if (strcmp(event_name, EVENT_CPU) == 0) {
        return PerfEvents::supported() ? (Engine*)&perf_events : (Engine*)&wall_clock;
    } else if (strcmp(event_name, EVENT_ALLOC) == 0) {
        return &alloc_tracer;
    } else if (strcmp(event_name, EVENT_LOCK) == 0) {
        return &lock_tracer;
    } else if (strcmp(event_name, EVENT_WALL) == 0) {
        return &wall_clock;
    } else if (strcmp(event_name, EVENT_ITIMER) == 0) {
        return &itimer;
    } else if (strchr(event_name, '.') != NULL) {
        return &instrument;
    } else {
        return &perf_events;
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
        fprintf(stderr, "WARNING: Install JVM debug symbols to improve profile accuracy\n");
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

    if (reset || _start_time == 0) {
        // Reset counters
        _total_samples = 0;
        _total_counter = 0;
        memset(_failures, 0, sizeof(_failures));
        memset(_hashes, 0, sizeof(_hashes));
        memset(_traces, 0, sizeof(_traces));
        memset(_methods, 0, sizeof(_methods));

        // Index 0 denotes special call trace with no frames
        _hashes[0] = (u64)-1;

        // Reset frame buffer
        _frame_buffer_index = 0;
        _frame_buffer_overflow = false;

        // Reset thread filter bitmaps
        _thread_filter.clear();

        // Reset thread names and IDs
        MutexLocker ml(_thread_names_lock);
        _thread_names.clear();
        _thread_ids.clear();
    }

    // (Re-)allocate frames
    if (_frame_buffer_size != args._framebuf) {
        _frame_buffer_size = args._framebuf;
        _frame_buffer = (ASGCT_CallFrame*)realloc(_frame_buffer, _frame_buffer_size * sizeof(ASGCT_CallFrame));
        if (_frame_buffer == NULL) {
            _frame_buffer_size = 0;
            return Error("Not enough memory to allocate frame buffer (try smaller framebuf)");
        }
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

    updateSymbols();

    _safe_mode = args._safe_mode | (VM::hotspot_version() ? 0 : HOTSPOT_ONLY);

    _add_thread_frame = args._threads && args._output != OUTPUT_JFR;
    _update_thread_names = (args._threads || args._output == OUTPUT_JFR) && VMThread::hasNativeId();
    _thread_filter.init(args._filter);

    _engine = selectEngine(args._event);
    _cstack = args._cstack == CSTACK_DEFAULT ? _engine->cstack() : args._cstack;
    if (_cstack == CSTACK_LBR && _engine != &perf_events) {
        return Error("Branch stack is supported only with PMU events");
    }

    if (args._output == OUTPUT_JFR) {
        error = _jfr.start(args._file);
        if (error) {
            return error;
        }
    }

    error = _engine->start(args);
    if (error) {
        _jfr.stop();
        return error;
    }

    // Thread events might be already enabled by PerfEvents::start
    switchThreadEvents(JVMTI_ENABLE);
    switchNativeMethodTraps(true);

    _state = RUNNING;
    _start_time = time(NULL);
    return Error::OK;
}

Error Profiler::stop() {
    MutexLocker ml(_state_lock);
    if (_state != RUNNING) {
        return Error("Profiler is not active");
    }

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

void Profiler::dumpSummary(std::ostream& out) {
    char buf[256];
    snprintf(buf, sizeof(buf),
            "--- Execution profile ---\n"
            "Total samples       : %lld\n",
            _total_samples);
    out << buf;
    
    double percent = 100.0 / _total_samples;
    for (int i = 1; i < ASGCT_FAILURE_TYPES; i++) {
        const char* err_string = asgctError(-i);
        if (err_string != NULL && _failures[i] > 0) {
            snprintf(buf, sizeof(buf), "%-20s: %lld (%.2f%%)\n", err_string, _failures[i], _failures[i] * percent);
            out << buf;
        }
    }
    out << std::endl;

    if (_frame_buffer_overflow) {
        out << "Frame buffer overflowed! Consider increasing its size." << std::endl;
    } else {
        double usage = 100.0 * _frame_buffer_index / _frame_buffer_size;
        out << "Frame buffer usage  : " << usage << "%" << std::endl;
    }
    out << std::endl;
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
    u64 unknown = 0;

    for (int i = 0; i < MAX_CALLTRACES; i++) {
        CallTraceSample& trace = _traces[i];
        if (trace._samples == 0 || excludeTrace(&fn, &trace)) continue;

        if (trace._num_frames == 0) {
            unknown += (args._counter == COUNTER_SAMPLES ? trace._samples : trace._counter);
            continue;
        }

        for (int j = trace._num_frames - 1; j >= 0; j--) {
            const char* frame_name = fn.name(_frame_buffer[trace._start_frame + j]);
            out << frame_name << (j == 0 ? ' ' : ';');
        }
        out << (args._counter == COUNTER_SAMPLES ? trace._samples : trace._counter) << "\n";
    }

    if (unknown != 0) {
        out << "[frame_buffer_overflow] " << unknown << "\n";
    }
}

void Profiler::dumpFlameGraph(std::ostream& out, Arguments& args, bool tree) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE || _engine == NULL) return;

    FlameGraph flamegraph(args._title, args._counter, args._width, args._height, args._minwidth, args._reverse);
    FrameName fn(args, args._style, _thread_names_lock, _thread_names);

    for (int i = 0; i < MAX_CALLTRACES; i++) {
        CallTraceSample& trace = _traces[i];
        if (trace._samples == 0 || excludeTrace(&fn, &trace)) continue;

        u64 samples = (args._counter == COUNTER_SAMPLES ? trace._samples : trace._counter);
        int num_frames = trace._num_frames;

        Trie* f = flamegraph.root();
        if (num_frames == 0) {
            f = f->addChild("[frame_buffer_overflow]", samples);
        } else if (args._reverse) {
            if (_add_thread_frame) {
                // Thread frames always come first
                num_frames--;
                const char* frame_name = fn.name(_frame_buffer[trace._start_frame + num_frames]);
                f = f->addChild(frame_name, samples);
            }

            for (int j = 0; j < num_frames; j++) {
                const char* frame_name = fn.name(_frame_buffer[trace._start_frame + j]);
                f = f->addChild(frame_name, samples);
            }
        } else {
            for (int j = num_frames - 1; j >= 0; j--) {
                const char* frame_name = fn.name(_frame_buffer[trace._start_frame + j]);
                f = f->addChild(frame_name, samples);
            }
        }
        f->addLeaf(samples);
    }

    flamegraph.dump(out, tree);
}

void Profiler::dumpTraces(std::ostream& out, Arguments& args) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE || _engine == NULL) return;

    FrameName fn(args, args._style | STYLE_DOTTED, _thread_names_lock, _thread_names);
    double percent = 100.0 / _total_counter;
    char buf[1024] = {0};

    CallTraceSample** traces = new CallTraceSample*[MAX_CALLTRACES];
    for (int i = 0; i < MAX_CALLTRACES; i++) {
        traces[i] = &_traces[i];
    }
    qsort(traces, MAX_CALLTRACES, sizeof(CallTraceSample*), CallTraceSample::comparator);

    int max_traces = args._dump_traces < MAX_CALLTRACES ? args._dump_traces : MAX_CALLTRACES;
    for (int i = 0; i < max_traces; i++) {
        CallTraceSample* trace = traces[i];
        if (trace->_samples == 0) break;
        if (excludeTrace(&fn, trace)) continue;

        snprintf(buf, sizeof(buf) - 1, "--- %lld %s (%.2f%%), %lld sample%s\n",
                 trace->_counter, _engine->units(), trace->_counter * percent,
                 trace->_samples, trace->_samples == 1 ? "" : "s");
        out << buf;

        if (trace->_num_frames == 0) {
            out << "  [ 0] [frame_buffer_overflow]\n";
        }

        for (int j = 0; j < trace->_num_frames; j++) {
            const char* frame_name = fn.name(_frame_buffer[trace->_start_frame + j]);
            snprintf(buf, sizeof(buf) - 1, "  [%2d] %s\n", j, frame_name);
            out << buf;
        }
        out << "\n";
    }

    delete[] traces;
}

void Profiler::dumpFlat(std::ostream& out, Arguments& args) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE || _engine == NULL) return;

    FrameName fn(args, args._style | STYLE_DOTTED, _thread_names_lock, _thread_names);
    double percent = 100.0 / _total_counter;
    char buf[1024] = {0};

    MethodSample** methods = new MethodSample*[MAX_CALLTRACES];
    for (int i = 0; i < MAX_CALLTRACES; i++) {
        methods[i] = &_methods[i];
    }
    qsort(methods, MAX_CALLTRACES, sizeof(MethodSample*), MethodSample::comparator);

    snprintf(buf, sizeof(buf) - 1, "%12s  percent  samples  top\n"
                                   "  ----------  -------  -------  ---\n", _engine->units());
    out << buf;

    int max_methods = args._dump_flat < MAX_CALLTRACES ? args._dump_flat : MAX_CALLTRACES;
    for (int i = 0; i < max_methods; i++) {
        MethodSample* method = methods[i];
        if (method->_samples == 0) break;

        const char* frame_name = fn.name(method->_method);
        snprintf(buf, sizeof(buf) - 1, "%12lld  %6.2f%%  %7lld  %s\n",
                 method->_counter, method->_counter * percent, method->_samples, frame_name);
        out << buf;
    }

    delete[] methods;
}

void Profiler::runInternal(Arguments& args, std::ostream& out) {
    switch (args._action) {
        case ACTION_START:
        case ACTION_RESUME: {
            Error error = start(args, args._action == ACTION_START);
            if (error) {
                out << error.message() << std::endl;
            } else {
                out << "Started [" << args._event << "] profiling" << std::endl;
            }
            break;
        }
        case ACTION_STOP: {
            Error error = stop();
            if (error) {
                out << error.message() << std::endl;
            } else {
                out << "Stopped profiling after " << uptime() << " seconds. No dump options specified" << std::endl;
            }
            break;
        }
        case ACTION_CHECK: {
            Error error = check(args);
            if (error) {
                out << error.message() << std::endl;
            } else {
                out << "OK" << std::endl;
            }
            break;
        }
        case ACTION_STATUS: {
            MutexLocker ml(_state_lock);
            if (_state == RUNNING) {
                out << "[" << _engine->name() << "] profiling is running for " << uptime() << " seconds" << std::endl;
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
                    dumpSummary(out);
                    if (args._dump_traces > 0) dumpTraces(out, args);
                    if (args._dump_flat > 0) dumpFlat(out, args);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void Profiler::run(Arguments& args) {
    if (args._file == NULL || args._output == OUTPUT_JFR) {
        runInternal(args, std::cout);
    } else {
        std::ofstream out(args._file, std::ios::out | std::ios::trunc);
        if (out.is_open()) {
            runInternal(args, out);
            out.close();
        } else {
            std::cerr << "Could not open " << args._file << std::endl;
        }
    }
}

void Profiler::shutdown(Arguments& args) {
    MutexLocker ml(_state_lock);

    // The last chance to dump profile before VM terminates
    if (_state == RUNNING && args._output != OUTPUT_NONE) {
        args._action = ACTION_DUMP;
        run(args);
    }

    _state = TERMINATED;
}
