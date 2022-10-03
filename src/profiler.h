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
#include <map>
#include <time.h>
#include "arch.h"
#include "arguments.h"
#include "callTraceStorage.h"
#include "codeCache.h"
#include "dictionary.h"
#include "engine.h"
#include "event.h"
#include "flightRecorder.h"
#include "log.h"
#include "mutex.h"
#include "spinLock.h"
#include "threadFilter.h"
#include "trap.h"
#include "vmEntry.h"


const char FULL_VERSION_STRING[] =
    "Async-profiler " PROFILER_VERSION " built on " __DATE__ "\n"
    "Copyright 2016-2021 Andrei Pangin\n";

const int MAX_NATIVE_FRAMES = 128;
const int RESERVED_FRAMES   = 4;
const int CONCURRENCY_LEVEL = 16;


union CallTraceBuffer {
    ASGCT_CallFrame _asgct_frames[1];
    jvmtiFrameInfo _jvmti_frames[1];
};


class FrameName;
class NMethod;
class StackContext;

enum State {
    NEW,
    IDLE,
    RUNNING,
    TERMINATED
};

class Profiler {
  private:
    Mutex _state_lock;
    State _state;
    Trap _begin_trap;
    Trap _end_trap;
    Mutex _thread_names_lock;
    // TODO: single map?
    std::map<int, std::string> _thread_names;
    std::map<int, jlong> _thread_ids;
    Dictionary _class_map;
    Dictionary _symbol_map;
    ThreadFilter _thread_filter;
    CallTraceStorage _call_trace_storage;
    FlightRecorder _jfr;
    Engine* _engine;
    Engine* _alloc_engine;
    int _event_mask;

    time_t _start_time;
    time_t _stop_time;
    int _epoch;
    WaitableMutex _timer_lock;
    void* _timer_id;

    u64 _total_samples;
    u64 _failures[ASGCT_FAILURE_TYPES];

    SpinLock _locks[CONCURRENCY_LEVEL];
    CallTraceBuffer* _calltrace_buffer[CONCURRENCY_LEVEL];
    int _max_stack_depth;
    int _safe_mode;
    CStack _cstack;
    bool _add_event_frame;
    bool _add_thread_frame;
    bool _add_sched_frame;
    bool _update_thread_names;
    volatile jvmtiEventMode _thread_events_state;

    SpinLock _stubs_lock;
    CodeCache _runtime_stubs;
    CodeCacheArray _native_libs;
    const void* _call_stub_begin;
    const void* _call_stub_end;

    // dlopen() hook support
    void** _dlopen_entry;
    static void* dlopen_hook(const char* filename, int flags);
    void switchLibraryTrap(bool enable);

    Error installTraps(const char* begin, const char* end);
    void uninstallTraps();

    void addJavaMethod(const void* address, int length, jmethodID method);
    void addRuntimeStub(const void* address, int length, const char* name);

    void onThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
    void onThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);

    const char* asgctError(int code);
    u32 getLockIndex(int tid);
    bool isAddressInCode(uintptr_t addr);
    int getNativeTrace(void* ucontext, ASGCT_CallFrame* frames, int event_type, int tid, StackContext* java_ctx);
    int getJavaTraceAsync(void* ucontext, ASGCT_CallFrame* frames, int max_depth, StackContext* java_ctx);
    int getJavaTraceJvmti(jvmtiFrameInfo* jvmti_frames, ASGCT_CallFrame* frames, int start_depth, int max_depth);
    int getJavaTraceInternal(jvmtiFrameInfo* jvmti_frames, ASGCT_CallFrame* frames, int max_depth);
    int convertFrames(jvmtiFrameInfo* jvmti_frames, ASGCT_CallFrame* frames, int num_frames);
    void fillFrameTypes(ASGCT_CallFrame* frames, int num_frames, NMethod* nmethod);
    void setThreadInfo(int tid, const char* name, jlong java_thread_id);
    void updateThreadName(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
    void updateJavaThreadNames();
    void updateNativeThreadNames();
    bool excludeTrace(FrameName* fn, CallTrace* trace);
    void mangle(const char* name, char* buf, size_t size);
    Engine* selectEngine(const char* event_name);
    Engine* selectAllocEngine(long alloc_interval, bool live);
    Engine* activeEngine();
    Error checkJvmCapabilities();

    time_t addTimeout(time_t start, int timeout);
    void startTimer();
    void stopTimer();
    void timerLoop(void* timer_id);
    static void timerThreadEntry(jvmtiEnv* jvmti, JNIEnv* jni, void* arg);

    void lockAll();
    void unlockAll();

    void dumpCollapsed(std::ostream& out, Arguments& args);
    void dumpFlameGraph(std::ostream& out, Arguments& args, bool tree);
    void dumpText(std::ostream& out, Arguments& args);

    static Profiler* const _instance;

  public:
    Profiler() :
        _state(NEW),
        _begin_trap(2),
        _end_trap(3),
        _thread_filter(),
        _call_trace_storage(),
        _jfr(),
        _start_time(0),
        _epoch(0),
        _timer_id(NULL),
        _max_stack_depth(0),
        _safe_mode(0),
        _thread_events_state(JVMTI_DISABLE),
        _stubs_lock(),
        _runtime_stubs("[stubs]"),
        _native_libs(),
        _call_stub_begin(NULL),
        _call_stub_end(NULL),
        _dlopen_entry(NULL) {

        for (int i = 0; i < CONCURRENCY_LEVEL; i++) {
            _calltrace_buffer[i] = NULL;
        }
    }

    static Profiler* instance() {
        return _instance;
    }

    u64 total_samples() { return _total_samples; }
    time_t uptime()     { return time(NULL) - _start_time; }

    Dictionary* classMap() { return &_class_map; }
    ThreadFilter* threadFilter() { return &_thread_filter; }
    CallTraceStorage* callTraceStorage() { return &_call_trace_storage; }

    Error run(Arguments& args);
    Error runInternal(Arguments& args, std::ostream& out);
    Error restart(Arguments& args);
    void shutdown(Arguments& args);
    Error check(Arguments& args);
    Error start(Arguments& args, bool reset);
    Error stop();
    Error flushJfr();
    Error dump(std::ostream& out, Arguments& args);
    void printUsedMemory(std::ostream& out);
    void switchThreadEvents(jvmtiEventMode mode);
    int convertNativeTrace(int native_frames, const void** callchain, ASGCT_CallFrame* frames);
    u32 recordSample(void* ucontext, u64 counter, jint event_type, Event* event);
    void recordExternalSample(u64 counter, Event* event, int tid, int num_frames, ASGCT_CallFrame* frames);
    void writeLog(LogLevel level, const char* message);
    void writeLog(LogLevel level, const char* message, size_t len);

    void updateSymbols(bool kernel_symbols);
    const void* resolveSymbol(const char* name);
    const char* getLibraryName(const char* native_symbol);
    CodeCache* findJvmLibrary(const char* lib_name);
    CodeCache* findLibraryByName(const char* lib_name);
    CodeCache* findLibraryByAddress(const void* address);
    const char* findNativeMethod(const void* address);

    void trapHandler(int signo, siginfo_t* siginfo, void* ucontext);
    static void segvHandler(int signo, siginfo_t* siginfo, void* ucontext);
    static void setupSignalHandlers();

    // CompiledMethodLoad is also needed to enable DebugNonSafepoints info by default
    static void JNICALL CompiledMethodLoad(jvmtiEnv* jvmti, jmethodID method,
                                           jint code_size, const void* code_addr,
                                           jint map_length, const jvmtiAddrLocationMap* map,
                                           const void* compile_info) {
        instance()->addJavaMethod(code_addr, code_size, method);
    }

    static void JNICALL DynamicCodeGenerated(jvmtiEnv* jvmti, const char* name,
                                             const void* address, jint length) {
        instance()->addRuntimeStub(address, length, name);
    }

    static void JNICALL ThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
        instance()->onThreadStart(jvmti, jni, thread);
    }

    static void JNICALL ThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
        instance()->onThreadEnd(jvmti, jni, thread);
    }

    friend class Recording;
};

#endif // _PROFILER_H
