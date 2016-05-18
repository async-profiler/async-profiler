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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include "asyncProfiler.h"

extern JavaVM* _vm;
extern jvmtiEnv* _jvmti;

Profiler Profiler::_instance;


static void sigprofHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    Profiler::_instance.recordSample(ucontext);
}


void CallTraceSample::assign(ASGCT_CallTrace* trace) {
    _call_count = 1;
    _num_frames = trace->num_frames;
    for (int i = 0; i < trace->num_frames; i++) {
        _frames[i] = trace->frames[i];
    }
}

char* CallTraceSample::format_class_name(char* class_name) {
    for (char* s = class_name; *s != 0; s++) {
        if (*s == '/') *s = '.';
    }
    class_name[strlen(class_name) - 1] = 0;
    return class_name + 1;
}

void CallTraceSample::dump(std::ostream& out) {
    char buf[1024];
    for (int i = 0; i < _num_frames; i++) {
        if (_frames[i].method_id != NULL) {
            char* name;
            char* sig;
            char* class_sig;
            jclass method_class;
            _jvmti->GetMethodName(_frames[i].method_id, &name, &sig, NULL);
            _jvmti->GetMethodDeclaringClass(_frames[i].method_id, &method_class);
            _jvmti->GetClassSignature(method_class, &class_sig, NULL);
            sprintf(buf, "  [%2d] %s.%s @%d\n", i, format_class_name(class_sig), name, _frames[i].bci);
            out << buf;
            _jvmti->Deallocate((unsigned char*)name);
            _jvmti->Deallocate((unsigned char*)sig);
            _jvmti->Deallocate((unsigned char*)class_sig);
        }
    }
}


u64 Profiler::hashCallTrace(ASGCT_CallTrace* trace) {
    const u64 M = 0xc6a4a7935bd1e995LL;
    const int R = 47;

    u64 h = trace->num_frames * M;

    for (int i = 0; i < trace->num_frames; i++) {
        u64 k = ((u64)trace->frames[i].bci << 32) ^ (u64)trace->frames[i].method_id;
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

void Profiler::storeCallTrace(ASGCT_CallTrace* trace) {
    u64 hash = hashCallTrace(trace);
    int bucket = (int)(hash % MAX_CALLTRACES);
    int i = bucket;

    do {
        if (_hashes[i] == hash && _samples[i]._call_count > 0) {
            _samples[i]._call_count++;
            return;
        } else if (_hashes[i] == 0) {
            break;
        }
        if (++i == MAX_CALLTRACES) i = 0;
    } while (i != bucket);

    _hashes[i] = hash;
    _samples[i].assign(trace);
}

void Profiler::recordSample(void* ucontext) {
    _calls_total++;

    JNIEnv* env;
    if (_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != 0) {
        _calls_non_java++;
        return;
    }

    ASGCT_CallFrame frames[MAX_FRAMES];
    ASGCT_CallTrace trace = {env, MAX_FRAMES, frames};
    AsyncGetCallTrace(&trace, trace.num_frames, ucontext);

    if (trace.num_frames > 0) {
        storeCallTrace(&trace);
    } else if (trace.num_frames == -2) {
        _calls_gc++;
    } else if (trace.num_frames == -9) {
        _calls_deopt++;
    } else {
        _calls_unknown++;
    }
}

void Profiler::setTimer(long sec, long usec) {
    bool enabled = sec | usec;

    struct sigaction sa;
    sa.sa_handler = enabled ? NULL : SIG_IGN;
    sa.sa_sigaction = enabled ? sigprofHandler : NULL;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, NULL);

    struct itimerval itv = {{sec, usec}, {sec, usec}};
    setitimer(ITIMER_PROF, &itv, NULL);
}

void Profiler::start() {
    if (_running) return;
    _running = true;

    _calls_total = _calls_non_java = _calls_gc = _calls_deopt = _calls_unknown = 0;
    memset(_hashes, 0, sizeof(_hashes));
    memset(_samples, 0, sizeof(_samples));

    setTimer(0, SAMPLING_INTERVAL);
}

void Profiler::stop() {
    if (!_running) return;
    _running = false;

    setTimer(0, 0);
}

void Profiler::dump(std::ostream& out, int max_traces) {
    if (_running) return;

    float percent = 100.0f / _calls_total;

    char buf[256];
    sprintf(buf,
        "--- Execution profile ---\n"
        "Total:    %d\n"
        "Non-Java: %d (%.2f%%)\n"
        "GC:       %d (%.2f%%)\n"
        "Deopt:    %d (%.2f%%)\n"
        "Unknown:  %d (%.2f%%)\n",
        _calls_total,
        _calls_non_java, _calls_non_java * percent,
        _calls_gc, _calls_gc * percent,
        _calls_deopt, _calls_deopt * percent,
        _calls_unknown, _calls_unknown * percent
    );
    out << buf;

    qsort(_samples, MAX_CALLTRACES, sizeof(CallTraceSample), CallTraceSample::comparator);
    if (max_traces > MAX_CALLTRACES) max_traces = MAX_CALLTRACES;

    for (int i = 0; i < max_traces; i++) {
        int samples = _samples[i]._call_count;
        if (samples > 0) {
            sprintf(buf, "\nSamples: %d (%.2f%%)\n", samples, samples * percent);
            out << buf;
            _samples[i].dump(out);
        }
    }
}
