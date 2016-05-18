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

#include <jvmti.h>
#include <iostream>

#define MAX_FRAMES         100
#define MAX_CALLTRACES     65536
#define MAX_TRACES_TO_DUMP 1000
#define SAMPLING_INTERVAL  10000

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

class CallTraceSample {
  private:
    int _call_count;
    int _num_frames;
    ASGCT_CallFrame _frames[MAX_FRAMES];

    static char* format_class_name(char* class_name);

  public:
    void assign(ASGCT_CallTrace* trace);
    void dump(std::ostream& out);

    static int comparator(const void* s1, const void* s2) {
        return ((CallTraceSample*)s2)->_call_count - ((CallTraceSample*)s1)->_call_count;
    }

    friend class Profiler;
};

class Profiler {
  private:
    bool _running;
    int _calls_total;
    int _calls_non_java;
    int _calls_gc;
    int _calls_deopt;
    int _calls_unknown;
    u64 _hashes[MAX_CALLTRACES];
    CallTraceSample _samples[MAX_CALLTRACES];

    u64 hashCallTrace(ASGCT_CallTrace* trace);
    void storeCallTrace(ASGCT_CallTrace* trace);
    void setTimer(long sec, long usec);

  public:
    static Profiler _instance;

    Profiler() : _running(false) {}

    bool is_running() { return _running; }

    void start();
    void stop();
    void dump(std::ostream& out, int max_traces);
    void recordSample(void* ucontext);
};


extern "C" JNIIMPORT void JNICALL
AsyncGetCallTrace(ASGCT_CallTrace* trace, jint depth, void* ucontext);
