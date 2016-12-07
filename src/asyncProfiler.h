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

#include <dlfcn.h>
#include <jvmti.h>
#include <iostream>

#define MAX_CALLTRACES 32768

#define DEFAULT_FRAME_BUFFER_SIZE 1024*1024
#define DEFAULT_INTERVAL          10
#define DEFAULT_DURATION          3600
#define DEFAULT_TRACES_TO_DUMP    500


typedef unsigned long long u64;

typedef struct {
    jint bci;
    jmethodID method_id;
} ASGCT_CallFrame;

typedef struct {
    JNIEnv* env;
    jint num_frames;
    ASGCT_CallFrame* frames;
} ASGCT_CallTrace;

typedef void (*ASGCTType)(ASGCT_CallTrace *, jint, void *);

extern "C" ASGCTType asgct;

class MethodName {
  private:
    char* _name;
    char* _sig;
    char* _class_sig;

  public:
    MethodName(jmethodID method);
    ~MethodName();

    char* holder()    { return _class_sig + 1; }
    char* name()      { return _name; }
    char* signature() { return _sig; }
};

class CallTraceSample {
  private:
    int _call_count;
    int _offset; // Offset in frame buffer
    int _num_frames;

  public:
    static int comparator(const void* s1, const void* s2) {
        return ((CallTraceSample*)s2)->_call_count - ((CallTraceSample*)s1)->_call_count;
    }

    friend class Profiler;
};

class MethodSample {
  private:
    int _call_count;
    jmethodID _method;

  public:
    static int comparator(const void* s1, const void* s2) {
        return ((MethodSample*)s2)->_call_count - ((MethodSample*)s1)->_call_count;
    }

    friend class Profiler;
};

class Profiler {
  private:
    bool _running;
    int _calls_total;
    // See ./hotspot/src/share/vm/prims/forte.cpp
    int _calls_non_java;
    int _calls_no_class_load;
    int _calls_gc_active;
    int _calls_unknown_not_java;
    int _calls_not_walkable_not_java;
    int _calls_unknown_java;
    int _calls_not_walkable_java;
    int _calls_unknown_state;
    int _calls_thread_exit;
    int _calls_deopt;
    int _calls_safepoint;
    int _calls_unknown;
    u64 _hashes[MAX_CALLTRACES];
    CallTraceSample _traces[MAX_CALLTRACES];
    MethodSample _methods[MAX_CALLTRACES];

    jmethodID *_frames;
    int _frameBufferSize;
    int _freeFrame;
    bool _frameBufferOverflow;

    // Seconds resolution is enough
    time_t _deadline;

    u64 hashCallTrace(ASGCT_CallTrace* trace);
    void storeCallTrace(ASGCT_CallTrace* trace);
    u64 hashMethod(jmethodID method);
    void storeMethod(jmethodID method);
    void checkDeadline();
    void setTimer(long sec, long usec);

  public:
    static Profiler _instance;

    Profiler() : 
        _running(false), 
        _frames(NULL), 
        _frameBufferSize(DEFAULT_FRAME_BUFFER_SIZE) {
    }

    bool is_running() { return _running; }
    int samples()     { return _calls_total; }

    void frameBufferSize(int size);
    void start(int interval, int duration = DEFAULT_DURATION);
    void stop();
    void summary(std::ostream& out);
    void dumpRawTraces(std::ostream& out);
    void dumpTraces(std::ostream& out, int max_traces);
    void dumpMethods(std::ostream& out);
    void recordSample(void* ucontext);
};
