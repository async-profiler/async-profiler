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

#ifndef _PROFILER_H
#define _PROFILER_H

#include <stdio.h>
#include <iostream>
#include <map>
#include <time.h>
#include "arch.h"
#include "arguments.h"
#include "engine.h"
#include "flightRecorder.h"
#include "mutex.h"
#include "spinLock.h"
#include "codeCache.h"
#include "vmEntry.h"


const char FULL_VERSION_STRING[] =
    "Async-profiler " PROFILER_VERSION " built on " __DATE__ "\n"
    "Copyright 2019 Andrei Pangin\n";

const int MAX_CALLTRACES    = 65536;
const int MAX_NATIVE_FRAMES = 128;
const int RESERVED_FRAMES   = 4;
const int MAX_NATIVE_LIBS   = 2048;
const int CONCURRENCY_LEVEL = 16;


static inline int cmp64(u64 a, u64 b) {
    return a > b ? 1 : a == b ? 0 : -1;
}


union CallTraceBuffer {
    ASGCT_CallFrame _asgct_frames[1];
    jvmtiFrameInfo _jvmti_frames[1];
};


class CallTraceSample {
  private:
    u64 _samples;
    u64 _counter;
    int _start_frame; // Offset in frame buffer
    int _num_frames;

  public:
    static int comparator(const void* s1, const void* s2) {
        return cmp64((*(CallTraceSample**)s2)->_counter, (*(CallTraceSample**)s1)->_counter);
    }

    friend class Profiler;
    friend class Recording;
};

class MethodSample {
  private:
    u64 _samples;
    u64 _counter;
    ASGCT_CallFrame _method;

  public:
    static int comparator(const void* s1, const void* s2) {
        return cmp64((*(MethodSample**)s2)->_counter, (*(MethodSample**)s1)->_counter);
    }

    friend class Profiler;
};


typedef jboolean JNICALL (*NativeLoadLibraryFunc)(JNIEnv*, jobject, jstring, jboolean);

enum State {
    IDLE,
    RUNNING,
    TERMINATED
};

class Profiler {
  private:
    Mutex _state_lock;
    State _state;
    Mutex _thread_names_lock;
    std::map<int, std::string> _thread_names;
    FlightRecorder _jfr;
    Engine* _engine;
    time_t _start_time;

    u64 _total_samples;
    u64 _total_counter;
    u64 _failures[ASGCT_FAILURE_TYPES];
    u64 _hashes[MAX_CALLTRACES];
    CallTraceSample _traces[MAX_CALLTRACES];
    MethodSample _methods[MAX_CALLTRACES];

    SpinLock _locks[CONCURRENCY_LEVEL];
    CallTraceBuffer* _calltrace_buffer[CONCURRENCY_LEVEL];
    ASGCT_CallFrame* _frame_buffer;
    int _frame_buffer_size;
    int _max_stack_depth;
    volatile int _frame_buffer_index;
    bool _frame_buffer_overflow;
    bool _threads;
    bool _sync_walk;
    volatile bool _thread_events_state;

    SpinLock _jit_lock;
    VmCodeCache _vm_code_cache;
    NativeLib* _native_libs[MAX_NATIVE_LIBS];
    volatile int _native_lib_count;

    JNINativeMethod _load_method;
    NativeLoadLibraryFunc _original_NativeLibrary_load;
    static jboolean JNICALL NativeLibraryLoadTrap(JNIEnv* env, jobject self, jstring name, jboolean builtin);
    void bindNativeLibraryLoad(NativeLoadLibraryFunc entry);

    void* (*_ThreadLocalStorage_thread)();
    jvmtiError (*_JvmtiEnv_GetStackTrace)(void* self, void* thread, jint start_depth, jint max_frame_count,
                                          jvmtiFrameInfo* frame_buffer, jint* count_ptr);

    void addJavaMethod(const void* address, int length, jmethodID method);
    void removeJavaMethod(const void* address, jmethodID method);
    void addRuntimeStub(const void* address, int length, const char* name);

    const char* asgctError(int code);
    const char* findNativeMethod(const void* address, bool& is_kernel);
    int getNativeTrace(void* ucontext, ASGCT_CallFrame* frames, int tid, bool* stopped_at_java_frame, const void** first_java_pc);
    int getJavaTraceAsync(void* ucontext, ASGCT_CallFrame* frames, int max_depth, const void** first_java_pc);
    int getJavaTraceJvmti(jvmtiFrameInfo* jvmti_frames, ASGCT_CallFrame* frames, int max_depth);
    int makeEventFrame(ASGCT_CallFrame* frames, jint event_type, jmethodID event);
    bool fillTopFrame(const void* pc, ASGCT_CallFrame* frame);
    bool addressInCode(const void* pc);
    u64 hashCallTrace(int num_frames, ASGCT_CallFrame* frames);
    int storeCallTrace(int num_frames, ASGCT_CallFrame* frames, u64 counter);
    bool tracesMatch(int num_frames1, ASGCT_CallFrame* frames1, int num_frames2, ASGCT_CallFrame* frames2);
    void copyToFrameBuffer(int num_frames, ASGCT_CallFrame* frames, CallTraceSample* trace);
    u64 hashMethod(jmethodID method, jint bci);
    void storeMethod(jmethodID method, jint bci, u64 counter);
    void initJvmtiFunctions(NativeLib* libjvm);
    void setThreadName(int tid, const char* name);
    void updateThreadName(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
    void updateAllThreadNames();
    Engine* selectEngine(const char* event_name);
    char getFrameType(jint bci);
    jint unOffsetBci(jint bci);

  public:
    static Profiler _instance;

    Profiler() :
        _state(IDLE),
        _jfr(),
        _frame_buffer(NULL),
        _frame_buffer_size(0),
        _max_stack_depth(0),
        _thread_events_state(JVMTI_DISABLE),
        _jit_lock(),
        _vm_code_cache(),
        _native_lib_count(0),
        _original_NativeLibrary_load(NULL),
        _ThreadLocalStorage_thread(NULL),
        _JvmtiEnv_GetStackTrace(NULL) {

        for (int i = 0; i < CONCURRENCY_LEVEL; i++) {
            _calltrace_buffer[i] = NULL;
        }
    }

    u64 total_samples() { return _total_samples; }
    u64 total_counter() { return _total_counter; }
    time_t uptime()     { return time(NULL) - _start_time; }

    void run(Arguments& args);
    void runInternal(Arguments& args, std::ostream& out);
    void shutdown(Arguments& args);
    Error start(Arguments& args, bool reset);
    Error stop();
    void switchThreadEvents(jvmtiEventMode mode);
    void dumpSummary(std::ostream& out);
    void dumpCollapsed(std::ostream& out, Arguments& args);
    void dumpFlameGraph(std::ostream& out, Arguments& args, bool tree);
    void dumpTraces(std::ostream& out, Arguments& args);
    void dumpFlat(std::ostream& out, Arguments& args);
    void recordSample(void* ucontext, u64 counter, jint event_type, jmethodID event);
    NativeLib* jvmLibrary();
    const void* findSymbol(const char* name);

    // CompiledMethodLoad is also needed to enable DebugNonSafepoints info by default
    static void JNICALL CompiledMethodLoad(jvmtiEnv* jvmti, jmethodID method,
                                           jint code_size, const void* code_addr,
                                           jint map_length, const jvmtiAddrLocationMap* map,
                                           const void* compile_info) {
        _instance.addJavaMethod(code_addr, code_size, method);
    }

    static void JNICALL CompiledMethodUnload(jvmtiEnv* jvmti, jmethodID method,
                                             const void* code_addr) {
        _instance.removeJavaMethod(code_addr, method);
    }

    static void JNICALL DynamicCodeGenerated(jvmtiEnv* jvmti, const char* name,
                                             const void* address, jint length) {
        _instance.addRuntimeStub(address, length, name);
    }

    static void JNICALL ThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
        _instance.updateThreadName(jvmti, jni, thread);
        _instance._engine->onThreadStart();
    }

    static void JNICALL ThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
        _instance.updateThreadName(jvmti, jni, thread);
        _instance._engine->onThreadEnd();
    }

    friend class Recording;
};

#endif // _PROFILER_H
