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
#include "profiler.h"
#include "perfEvent.h"
#include "stackFrame.h"


Profiler Profiler::_instance;

static void sigprofHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    Profiler::_instance.recordSample(ucontext);
    PerfEvent::reenable(siginfo);
}

static inline u64 atomicInc(u64& var) {
    return __sync_fetch_and_add(&var, 1);
}


void Profiler::frameBufferSize(int size) {
    if (size >= 0) {
        _frame_buffer_size = size;
    } else {
        std::cerr << "Ignoring frame buffer size " << size << std::endl;
    }
}

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

void Profiler::storeCallTrace(int num_frames, ASGCT_CallFrame* frames) {
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
        if (i == bucket) return;           // the table is full
    }
    
    // CallTrace hash found => atomically increment counter
    atomicInc(_traces[i]._counter);
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
        _frame_buffer[start_frame++] = frames[i].method_id;
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

void Profiler::storeMethod(jmethodID method) {
    u64 hash = hashMethod(method);
    int bucket = (int)(hash % MAX_CALLTRACES);
    int i = bucket;

    while (_methods[i]._method != method) {
        if (_methods[i]._method == NULL) {
            if (__sync_bool_compare_and_swap(&_methods[i]._method, NULL, method)) {
                break;
            }
            continue;
        }
        
        if (++i == MAX_CALLTRACES) i = 0;  // move to next slot
        if (i == bucket) return;           // the table is full
    }

    // Method found => atomically increment counter
    atomicInc(_methods[i]._counter);
}

void Profiler::checkDeadline() {
    if (time(NULL) > _deadline) {
        const char error[] = "Profiling duration elapsed. Disabling the "
                             "profiler automatically is not currently "
                             "supported. Use 'stop' explicitly.\n";
        ssize_t w = write(STDERR_FILENO, error, sizeof(error) - 1);
        (void) w;
        _deadline = INT_MAX;    // Prevent further invocations

        // FIXME Stopping the profiler is not safe from a signal handler. We
        // need to refactor this to a separate thread that sleeps for the
        // specified duration, or issue the stop command from a separate
        // thread or after returning from the signal.
    }
}

jmethodID Profiler::findNativeMethod(const void* address) {
    for (int i = 0; i < _native_libs; i++) {
        if (_native_code[i]->contains(address)) {
            return _native_code[i]->binary_search(address);
        }
    }
    return NULL;
}

int Profiler::getNativeTrace(void* ucontext, ASGCT_CallFrame* frames) {
    const void* native_callchain[MAX_NATIVE_FRAMES];
    int native_frames = PerfEvent::getCallChain(native_callchain, MAX_NATIVE_FRAMES);

    for (int i = 0; i < native_frames; i++) {
        if (_java_code.contains(native_callchain[i])) {
            return i;
        }
        frames[i].method_id = findNativeMethod(native_callchain[i]);
    }

    return native_frames;
}

int Profiler::getJavaTrace(void* ucontext, ASGCT_CallFrame* frames, int native_frames) {
    JNIEnv* jni = VM::jni();
    if (jni == NULL) {
        atomicInc(_failures[-ticks_no_Java_frame]);
        return 0;
    }

    ASGCT_CallTrace trace = {jni, 0, frames + native_frames};
    VM::asyncGetCallTrace(&trace, MAX_STACK_FRAMES - native_frames, ucontext);

    if (trace.num_frames == ticks_unknown_Java) {
        // If current Java stack is not walkable (e.g. the top frame is not fully constructed),
        // try to manually pop the top frame off, hoping that the previous frame is walkable.
        // This is a temporary workaround for AsyncGetCallTrace issues,
        // see https://bugs.openjdk.java.net/browse/JDK-8178287
        StackFrame top_frame(ucontext);
        if (top_frame.pop()) {
            // Add one slot to manually insert top method
            trace.frames[0].method_id = _java_code.linear_search(top_frame.pc());
            trace.frames++;

            // Retry with the fixed context
            VM::asyncGetCallTrace(&trace, MAX_STACK_FRAMES - 1 - native_frames, ucontext);

            if (trace.num_frames > 0) {
                return trace.num_frames + 1;
            }

            // Restore previous context
            trace.num_frames = ticks_unknown_Java;
        }
    }

    if (trace.num_frames > 0) {
        return trace.num_frames;
    }

    // Record failure
    int type = -trace.num_frames < FAILURE_TYPES ? -trace.num_frames : -ticks_unknown_state;
    atomicInc(_failures[type]);
    return 0;
}

void Profiler::recordSample(void* ucontext) {
    checkDeadline();

    u64 lock_index = atomicInc(_samples) % CONCURRENCY_LEVEL;
    if (!_locks[lock_index].tryLock()) {
        atomicInc(_failures[-ticks_skipped]);  // too many concurrent signals already
        return;
    }

    ASGCT_CallFrame* frames = _asgct_buffer[lock_index];
    int num_frames = getNativeTrace(ucontext, frames);
    num_frames += getJavaTrace(ucontext, frames, num_frames);

    if (num_frames > 0) {
        storeCallTrace(num_frames, frames);
        storeMethod(frames[0].method_id);
    }

    _locks[lock_index].unlock();
}

void Profiler::resetSymbols() {
    for (int i = 0; i < _native_libs; i++) {
        delete _native_code[i];
    }
    _native_libs = Symbols::parseMaps(_native_code, MAX_NATIVE_LIBS);
}

void Profiler::setSignalHandler() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = NULL;
    sa.sa_sigaction = sigprofHandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;

    if (sigaction(SIGPROF, &sa, NULL)) {
        perror("sigaction failed");
    }
}

void Profiler::start(int interval, int duration) {
    if (_running || interval <= 0 || duration <= 0) return;
    _running = true;

    _samples = 0;
    memset(_failures, 0, sizeof(_failures));
    memset(_hashes, 0, sizeof(_hashes));
    memset(_traces, 0, sizeof(_traces));
    memset(_methods, 0, sizeof(_methods));

    // Reset frames
    free(_frame_buffer);
    _frame_buffer = (jmethodID*)malloc(_frame_buffer_size * sizeof(jmethodID));
    _frame_buffer_index = 0;
    _frame_buffer_overflow = false;

    resetSymbols();

    _deadline = time(NULL) + duration;
    setSignalHandler();
    
    PerfEvent::start(interval);
}

void Profiler::stop() {
    if (!_running) return;
    _running = false;

    PerfEvent::stop();

    if (_frame_buffer_overflow) {
        std::cerr << "Frame buffer overflowed with size " << _frame_buffer_size
                << ". Consider increasing its size." << std::endl;
    } else {
        std::cout << "Frame buffer usage "
                << _frame_buffer_index << "/" << _frame_buffer_size
                << "="
                << 100.0 * _frame_buffer_index / _frame_buffer_size << "%" << std::endl;
    }
}

void Profiler::summary(std::ostream& out) {
    static const char* title[FAILURE_TYPES] = {
        "Non-Java:",
        "JVM not initialized:",
        "GC active:",
        "Unknown (native):",
        "Not walkable (native):",
        "Unknown (Java):",
        "Not walkable (Java):",
        "Unknown state:",
        "Thread exit:",
        "Deopt:",
        "Safepoint:",
        "Skipped:"
    };

    double percent = 100.0 / _samples;
    char buf[256];
    snprintf(buf, sizeof(buf),
            "--- Execution profile ---\n"
            "Total:                 %lld\n",
            _samples);
    out << buf;
    
    for (int i = 0; i < FAILURE_TYPES; i++) {
        if (_failures[i] > 0) {
            snprintf(buf, sizeof(buf), "%-22s %lld (%.2f%%)\n", title[i], _failures[i], _failures[i] * percent);
            out << buf;
        }
    }

    out << std::endl;
}

/*
 * Dumping in lightweight-java-profiler format:
 * 
 * <samples> <frames>   <frame1>
 *                      <frame2>
 *                      ...
 *                      <framen>
 * ...
 * Total ...
 */
void Profiler::dumpRawTraces(std::ostream& out) {
    if (_running) return;

    for (int i = 0; i < MAX_CALLTRACES; i++) {
        u64 samples = _traces[i]._counter;
        if (samples == 0) continue;
        
        out << samples << '\t' << _traces[i]._num_frames << '\t';

        CallTraceSample& trace = _traces[i];
        for (int j = 0; j < trace._num_frames; j++) {
            if (j != 0) out << "\t\t";

            jmethodID method = _frame_buffer[trace._start_frame + j];
            MethodName mn(method, "::");
            out << mn.toString() << std::endl;
        }
    }
    
    out << "Total" << std::endl;
}

void Profiler::dumpTraces(std::ostream& out, int max_traces) {
    if (_running) return;

    double percent = 100.0 / _samples;
    char buf[1024];

    qsort(_traces, MAX_CALLTRACES, sizeof(CallTraceSample), CallTraceSample::comparator);
    if (max_traces > MAX_CALLTRACES) max_traces = MAX_CALLTRACES;

    for (int i = max_traces - 1; i >= 0; i--) {
        u64 samples = _traces[i]._counter;
        if (samples == 0) continue;

        snprintf(buf, sizeof(buf), "Samples: %lld (%.2f%%)\n", samples, samples * percent);
        out << buf;

        CallTraceSample& trace = _traces[i];
        for (int j = 0; j < trace._num_frames; j++) {
            jmethodID method = _frame_buffer[trace._start_frame + j];
            MethodName mn(method);
            snprintf(buf, sizeof(buf), "  [%2d] %s\n", j, mn.toString());
            out << buf;
        }
        out << "\n";
    }
}

void Profiler::dumpMethods(std::ostream& out) {
    if (_running) return;

    double percent = 100.0 / _samples;
    char buf[1024];

    qsort(_methods, MAX_CALLTRACES, sizeof(MethodSample), MethodSample::comparator);

    for (int i = MAX_CALLTRACES - 1; i >= 0; i--) {
        u64 samples = _methods[i]._counter;
        if (samples == 0) continue;

        MethodName mn(_methods[i]._method);
        snprintf(buf, sizeof(buf), "%10lld (%.2f%%) %s\n", samples, samples * percent, mn.toString());
        out << buf;
    }
}
