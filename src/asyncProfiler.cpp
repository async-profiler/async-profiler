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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/param.h>
#include "asyncProfiler.h"
#include "vmEntry.h"

ASGCTType asgct;
int maxFrames = DEFAULT_MAX_FRAMES;
Profiler Profiler::_instance;

static void sigprofHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    Profiler::_instance.recordSample(ucontext);
}


MethodName::MethodName(jmethodID method) {
    jclass method_class;
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->GetMethodName(method, &_name, &_sig, NULL);
    jvmti->GetMethodDeclaringClass(method, &method_class);
    jvmti->GetClassSignature(method_class, &_class_sig, NULL);

    char* s;
    for (s = _class_sig; *s; s++) {
        if (*s == '/') *s = '.';
    }
    s[-1] = 0;
}

MethodName::~MethodName() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->Deallocate((unsigned char*)_name);
    jvmti->Deallocate((unsigned char*)_sig);
    jvmti->Deallocate((unsigned char*)_class_sig);
}


void CallTraceSample::assign(ASGCT_CallTrace* trace) {
    _call_count = 1;
    _num_frames = MIN(trace->num_frames, maxFrames);
    
    const int offset = trace->num_frames - _num_frames;    
    for (int i = 0; i < _num_frames; i++) {
        _frames[i] = trace->frames[offset + i];
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
        if (_hashes[i] == hash && _traces[i]._call_count > 0) {
            _traces[i]._call_count++;
            return;
        } else if (_hashes[i] == 0) {
            break;
        }
        if (++i == MAX_CALLTRACES) i = 0;
    } while (i != bucket);

    _hashes[i] = hash;
    _traces[i].assign(trace);
}

u64 Profiler::hashMethod(jmethodID method) {
    const u64 M = 0xc6a4a7935bd1e995LL;
    const int R = 17;

    u64 h = (u64)method;

    h ^= h >> R;
    h *= M;
    h ^= h >> R;

    return h;
}

void Profiler::storeMethod(jmethodID method) {
    u64 hash = hashMethod(method);
    int bucket = (int)(hash % MAX_CALLTRACES);
    int i = bucket;

    do {
        if (_methods[i]._method == method) {
            _methods[i]._call_count++;
            return;
        } else if (_methods[i]._method == NULL) {
            break;
        }
        if (++i == MAX_CALLTRACES) i = 0;
    } while (i != bucket);

    _methods[i]._call_count = 1;
    _methods[i]._method = method;
}

void Profiler::recordSample(void* ucontext) {
    _calls_total++;

    JNIEnv* jni = VM::jni();
    if (jni == NULL) {
        _calls_non_java++;
        return;
    }

    const int FRAME_BUFFER_SIZE = 4096;
    ASGCT_CallFrame frames[FRAME_BUFFER_SIZE];
    ASGCT_CallTrace trace = {jni, FRAME_BUFFER_SIZE, frames};
    if (asgct == NULL) {
        const char ERROR[] = "No AsyncGetCallTrace";
        write(STDOUT_FILENO, ERROR, sizeof (ERROR));
        return;
    }

    (*asgct)(&trace, trace.num_frames, ucontext);

    if (trace.num_frames > 0) {
        storeCallTrace(&trace);
        storeMethod(frames[0].method_id);
    // See ticks_* enum values in http://hg.openjdk.java.net/jdk8/jdk8/hotspot/file/tip/src/share/vm/prims/forte.cpp
    } else if (trace.num_frames == 0) {
        _calls_non_java++;
    } else if (trace.num_frames == -1) {
        _calls_no_class_load++;
    } else if (trace.num_frames == -2) {
        _calls_gc_active++;
    } else if (trace.num_frames == -3) {
        _calls_unknown_not_java++;
    } else if (trace.num_frames == -4) {
        _calls_not_walkable_not_java++;
    } else if (trace.num_frames == -5) {
        _calls_unknown_java++;
    } else if (trace.num_frames == -6) {
        _calls_not_walkable_java++;
    } else if (trace.num_frames == -7) {
        _calls_unknown_state++;
    } else if (trace.num_frames == -8) {
        _calls_thread_exit++;
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
    if (sigaction(SIGPROF, &sa, NULL) != 0)
        perror("couldn't install signal handler");

    struct itimerval itv = {{sec, usec}, {sec, usec}};
    if (setitimer(ITIMER_PROF, &itv, NULL) != 0)
        perror("couldn't start timer");
}

void Profiler::start(long interval) {
    if (_running) return;
    _running = true;

    _calls_total = 0;
    _calls_non_java = 0;
    _calls_no_class_load = 0;
    _calls_gc_active = 0;
    _calls_unknown_not_java = 0;
    _calls_not_walkable_not_java = 0;
    _calls_unknown_java = 0;
    _calls_not_walkable_java = 0;
    _calls_unknown_state = 0;
    _calls_thread_exit = 0;
    _calls_deopt = 0;
    _calls_safepoint = 0;
    _calls_unknown = 0;
    memset(_hashes, 0, sizeof(_hashes));
    for (int i = 0; i < MAX_CALLTRACES; i++) {
        free(_traces[i]._frames);
        _traces[i]._frames = NULL;
    }
    memset(_traces, 0, sizeof(_traces));
    for (int i = 0; i < MAX_CALLTRACES; i++)
        _traces[i]._frames = (ASGCT_CallFrame *) malloc(maxFrames * sizeof(ASGCT_CallFrame));
    memset(_methods, 0, sizeof(_methods));

    setTimer(interval / 1000, (interval % 1000) * 1000);
}

void Profiler::stop() {
    if (!_running) return;
    _running = false;

    setTimer(0, 0);
}

void nonzero_summary(std::ostream& out, const char* fmt, int calls, float percent) {
    char buf[256];
    if (calls > 0) {
        snprintf(buf, sizeof (buf),
                fmt,
                calls, calls * percent);
        out << buf;
    }
}

void Profiler::summary(std::ostream& out) {
    float percent = 100.0f / _calls_total;
    char buf[256];
    snprintf(buf, sizeof(buf),
            "--- Execution profile ---\n"
            "Total:               %d\n",
            _calls_total);
    out << buf;
    
    nonzero_summary(out,
            "No Java frame:       %d (%.2f%%)\n",
            _calls_non_java, _calls_non_java * percent);
    nonzero_summary(out,
            "No class load:       %d (%.2f%%)\n",
            _calls_no_class_load, _calls_no_class_load * percent);
    nonzero_summary(out,
            "GC active:           %d (%.2f%%)\n",
            _calls_gc_active, _calls_gc_active * percent);
    nonzero_summary(out,
            "Unknown (non-Java):  %d (%.2f%%)\n",
            _calls_unknown_not_java, _calls_unknown_not_java * percent);
    nonzero_summary(out,
            "Not walkable (nonJ): %d (%.2f%%)\n",
            _calls_not_walkable_not_java, _calls_not_walkable_not_java * percent);
    nonzero_summary(out,
            "Unknown Java:        %d (%.2f%%)\n",
            _calls_unknown_java, _calls_unknown_java * percent);
    nonzero_summary(out,
            "Not walkable (Java): %d (%.2f%%)\n",
            _calls_not_walkable_java, _calls_not_walkable_java * percent);
    nonzero_summary(out,
            "Unknown state:       %d (%.2f%%)\n",
            _calls_unknown_state, _calls_unknown_state * percent);
    nonzero_summary(out,
            "Thread exit:         %d (%.2f%%)\n",
            _calls_thread_exit, _calls_thread_exit * percent);
    nonzero_summary(out,
            "Deopt:               %d (%.2f%%)\n",
            _calls_deopt, _calls_deopt * percent);
    nonzero_summary(out,
            "Safepoint:           %d (%.2f%%)\n",
            _calls_safepoint, _calls_safepoint * percent);
    nonzero_summary(out,
            "Unknown:             %d (%.2f%%)\n",
            _calls_unknown, _calls_unknown * percent);
    
    out << std::endl;
}

/*
 * Dumping in lightweight-java-profiler format:
 * 
 * <samples> <frames>   <frame1>
 *                      <frame2>
 *                      ...
 *                      <framen>
 */
void Profiler::dumpRawTraces(std::ostream& out) {
    if (_running) return;

    for (int i = 0; i < MAX_CALLTRACES; i++) {
        const int samples = _traces[i]._call_count;
        if (samples == 0) continue;
        
        out << samples << '\t' << _traces[i]._num_frames << '\t';

        for (int j = 0; j < _traces[i]._num_frames; j++) {
            ASGCT_CallFrame* frame = &_traces[i]._frames[j];
            if (frame->method_id != NULL) {
                if (j != 0) {
                    out << "\t\t";
                }

                MethodName mn(frame->method_id);
                out << mn.holder() << "::" << mn.name() << std::endl;
            }
        }
    }
}

void Profiler::dumpTraces(std::ostream& out, int max_traces) {
    if (_running) return;

    float percent = 100.0f / _calls_total;
    char buf[1024];

    qsort(_traces, MAX_CALLTRACES, sizeof(CallTraceSample), CallTraceSample::comparator);
    if (max_traces > MAX_CALLTRACES) max_traces = MAX_CALLTRACES;

    for (int i = 0; i < max_traces; i++) {
        int samples = _traces[i]._call_count;
        if (samples == 0) break;

        snprintf(buf, sizeof(buf), "Samples: %d (%.2f%%)\n", samples, samples * percent);
        out << buf;

        for (int j = 0; j < _traces[i]._num_frames; j++) {
            ASGCT_CallFrame* frame = &_traces[i]._frames[j];
            if (frame->method_id != NULL) {
                MethodName mn(frame->method_id);
                snprintf(buf, sizeof(buf), "  [%2d] %s.%s @%d\n", j, mn.holder(), mn.name(), frame->bci);
                out << buf;
            }
        }
        out << "\n";
    }
}

void Profiler::dumpMethods(std::ostream& out) {
    if (_running) return;

    float percent = 100.0f / _calls_total;
    char buf[1024];

    qsort(_methods, MAX_CALLTRACES, sizeof(MethodSample), MethodSample::comparator);

    for (int i = 0; i < MAX_CALLTRACES; i++) {
        int samples = _methods[i]._call_count;
        if (samples == 0) break;

        MethodName mn(_methods[i]._method);
        snprintf(buf, sizeof(buf), "%6d (%.2f%%) %s.%s\n", samples, samples * percent, mn.holder(), mn.name());
        out << buf;
    }
}
