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

#include <iostream>
#include <pthread.h>
#include <time.h>
#include "arch.h"
#include "arguments.h"
#include "engine.h"
#include "spinLock.h"
#include "codeCache.h"
#include "vmEntry.h"


const int MAX_CALLTRACES    = 65536;
const int MAX_STACK_FRAMES  = 2048;
const int MAX_NATIVE_FRAMES = 128;
const int MAX_NATIVE_LIBS   = 2048;
const int CONCURRENCY_LEVEL = 16;


static inline int cmp64(u64 a, u64 b) {
    return a > b ? 1 : a == b ? 0 : -1;
}


class CallTraceSample {
  private:
    u64 _samples;
    u64 _counter;
    int _start_frame; // Offset in frame buffer
    int _num_frames;

  public:
    static int comparator(const void* s1, const void* s2) {
        return cmp64(((CallTraceSample*)s2)->_counter, ((CallTraceSample*)s1)->_counter);
    }

    friend class Profiler;
};

class MethodSample {
  private:
    u64 _samples;
    u64 _counter;
    ASGCT_CallFrame _method;

  public:
    static int comparator(const void* s1, const void* s2) {
        return cmp64(((MethodSample*)s2)->_counter, ((MethodSample*)s1)->_counter);
    }

    friend class Profiler;
};


class MutexLocker {
  private:
    pthread_mutex_t* _mutex;

  public:
    MutexLocker(pthread_mutex_t& mutex) : _mutex(&mutex) {
        pthread_mutex_lock(_mutex);
    }

    ~MutexLocker() {
        pthread_mutex_unlock(_mutex);
    }
};


enum State {
    IDLE,
    RUNNING,
    TERMINATED
};

class Profiler {
  private:

    // See hotspot/src/share/vm/prims/forte.cpp
    enum {
        ticks_no_Java_frame         =  0,
        ticks_no_class_load         = -1,
        ticks_GC_active             = -2,
        ticks_unknown_not_Java      = -3,
        ticks_not_walkable_not_Java = -4,
        ticks_unknown_Java          = -5,
        ticks_not_walkable_Java     = -6,
        ticks_unknown_state         = -7,
        ticks_thread_exit           = -8,
        ticks_deopt                 = -9,
        ticks_safepoint             = -10,
        ticks_skipped               = -11,
        FAILURE_TYPES               = 12
    };

    pthread_mutex_t _state_lock;
    State _state;
    Engine* _engine;
    time_t _start_time;

    u64 _total_samples;
    u64 _total_counter;
    u64 _failures[FAILURE_TYPES];
    u64 _hashes[MAX_CALLTRACES];
    CallTraceSample _traces[MAX_CALLTRACES];
    MethodSample _methods[MAX_CALLTRACES];

    SpinLock _locks[CONCURRENCY_LEVEL];
    ASGCT_CallFrame _calltrace_buffer[CONCURRENCY_LEVEL][MAX_STACK_FRAMES];
    ASGCT_CallFrame* _frame_buffer;
    int _frame_buffer_size;
    volatile int _frame_buffer_index;
    bool _frame_buffer_overflow;
    bool _threads;

    SpinLock _jit_lock;
    const void* _jit_min_address;
    const void* _jit_max_address;
    CodeCache _java_methods;
    NativeCodeCache _runtime_stubs;
    NativeCodeCache* _native_libs[MAX_NATIVE_LIBS];
    int _native_lib_count;

    void addJavaMethod(const void* address, int length, jmethodID method);
    void removeJavaMethod(const void* address, jmethodID method);
    void addRuntimeStub(const void* address, int length, const char* name);
    void updateJitRange(const void* min_address, const void* max_address);

    const char* findNativeMethod(const void* address);
    int getNativeTrace(int tid, ASGCT_CallFrame* frames);
    int getJavaTraceAsync(void* ucontext, ASGCT_CallFrame* frames, int max_depth);
    int makeEventFrame(ASGCT_CallFrame* frames, jint event_type, jmethodID event);
    bool fillTopFrame(const void* pc, ASGCT_CallFrame* frame);
    u64 hashCallTrace(int num_frames, ASGCT_CallFrame* frames);
    void storeCallTrace(int num_frames, ASGCT_CallFrame* frames, u64 counter);
    void copyToFrameBuffer(int num_frames, ASGCT_CallFrame* frames, CallTraceSample* trace);
    u64 hashMethod(jmethodID method);
    void storeMethod(jmethodID method, jint bci, u64 counter);
    void initStateLock();
    void resetSymbols();
    void setSignalHandler();
    void runInternal(Arguments& args, std::ostream& out);

  public:
    static Profiler _instance;

    Profiler() :
        _state(IDLE),
        _frame_buffer(NULL),
        _jit_lock(),
        _jit_min_address((const void*)-1),
        _jit_max_address((const void*)0),
        _java_methods(),
        _runtime_stubs("[stubs]"),
        _native_lib_count(0) {
        initStateLock();
    }

    u64 total_samples() { return _total_samples; }
    u64 total_counter() { return _total_counter; }
    time_t uptime()     { return time(NULL) - _start_time; }

    void run(Arguments& args);
    void shutdown(Arguments& args);
    Error start(const char* event, long interval, int frame_buffer_size, bool threads);
    Error stop();
    void dumpSummary(std::ostream& out);
    void dumpCollapsed(std::ostream& out, Counter counter);
    void dumpFlameGraph(std::ostream& out, Counter counter, Arguments& args);
    void dumpTraces(std::ostream& out, int max_traces);
    void dumpFlat(std::ostream& out, int max_methods);
    void recordSample(void* ucontext, u64 counter, jint event_type, jmethodID event);
    NativeCodeCache* jvmLibrary();

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
};

#endif // _PROFILER_H
