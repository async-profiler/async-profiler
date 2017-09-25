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
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "profiler.h"
#include "perfEvent.h"
#include "allocTracer.h"
#include "frameName.h"
#include "stackFrame.h"
#include "symbols.h"


Profiler Profiler::_instance;

static inline u64 atomicInc(u64& var, u64 increment = 1) {
    return __sync_fetch_and_add(&var, increment);
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

void Profiler::storeCallTrace(int num_frames, ASGCT_CallFrame* frames, u64 counter) {
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
    atomicInc(_traces[i]._samples);
    atomicInc(_traces[i]._counter, counter);
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
    _java_methods.add(address, length, method);
    updateJitRange(address, (const char*)address + length);
    _jit_lock.unlock();
}

void Profiler::removeJavaMethod(const void* address, jmethodID method) {
    _jit_lock.lock();
    _java_methods.remove(address, method);
    _jit_lock.unlock();
}

void Profiler::addRuntimeStub(const void* address, int length, const char* name) {
    _jit_lock.lock();
    _runtime_stubs.add(address, length, name);
    updateJitRange(address, (const char*)address + length);
    _jit_lock.unlock();
}

void Profiler::updateJitRange(const void* min_address, const void* max_address) {
    if (min_address < _jit_min_address) _jit_min_address = min_address;
    if (max_address > _jit_max_address) _jit_max_address = max_address;
}

NativeCodeCache* Profiler::jvmLibrary() {
    const void* asyncGetCallTraceAddr = (const void*)VM::_asyncGetCallTrace;
    for (int i = 0; i < _native_lib_count; i++) {
        if (_native_libs[i]->contains(asyncGetCallTraceAddr)) {
            return _native_libs[i];
        }
    }
    return NULL;
}

const char* Profiler::findNativeMethod(const void* address) {
    for (int i = 0; i < _native_lib_count; i++) {
        if (_native_libs[i]->contains(address)) {
            return _native_libs[i]->binarySearch(address);
        }
    }
    return NULL;
}

int Profiler::getNativeTrace(void* ucontext, ASGCT_CallFrame* frames) {
    const void* native_callchain[MAX_NATIVE_FRAMES];
    int native_frames = PerfEvents::getCallChain(native_callchain, MAX_NATIVE_FRAMES);

    for (int i = 0; i < native_frames; i++) {
        const void* address = native_callchain[i];
        if (address >= _jit_min_address && address < _jit_max_address) {
            return i;
        }
        frames[i].bci = BCI_NATIVE_FRAME;
        frames[i].method_id = (jmethodID)findNativeMethod(address);
    }

    return native_frames;
}

int Profiler::getJavaTrace(void* ucontext, ASGCT_CallFrame* frames, int max_depth) {
    JNIEnv* jni = VM::jni();
    if (jni == NULL) {
        atomicInc(_failures[-ticks_no_Java_frame]);
        return 0;
    }

    ASGCT_CallTrace trace = {jni, 0, frames};
    VM::_asyncGetCallTrace(&trace, max_depth, ucontext);

    if (trace.num_frames == ticks_unknown_Java) {
        // If current Java stack is not walkable (e.g. the top frame is not fully constructed),
        // try to manually pop the top frame off, hoping that the previous frame is walkable.
        // This is a temporary workaround for AsyncGetCallTrace issues,
        // see https://bugs.openjdk.java.net/browse/JDK-8178287
        StackFrame top_frame(ucontext);
        uintptr_t pc = top_frame.pc(),
                  sp = top_frame.sp(),
                  fp = top_frame.fp();

        if (top_frame.pop()) {
            // Guess top method by PC and insert it manually into the call trace
            if (fillTopFrame((const void*)pc, trace.frames)) {
                trace.frames++;
                max_depth--;
            }

            // Retry with the fixed context
            VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
            top_frame.restore(pc, sp, fp);

            if (trace.num_frames > 0) {
                return trace.num_frames + (trace.frames - frames);
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

int Profiler::makeEventFrame(ASGCT_CallFrame* frames, jint event_type, jmethodID event) {
    if (event == NULL) {
        return 0;
    }

    frames[0].bci = event_type;
    frames[0].method_id = event;
    return 1;
}

bool Profiler::fillTopFrame(const void* pc, ASGCT_CallFrame* frame) {
    jmethodID method = NULL;
    _jit_lock.lockShared();

    // Check if PC lies within JVM's compiled code cache
    if (pc >= _jit_min_address && pc < _jit_max_address) {
        if ((method = _java_methods.find(pc)) != NULL) {
            // PC belong to a JIT compiled method
            frame->bci = 0;
            frame->method_id = method;
        } else if ((method = _runtime_stubs.find(pc)) != NULL) {
            // PC belongs to a VM runtime stub
            frame->bci = BCI_NATIVE_FRAME;
            frame->method_id = method;
        }
    }

    _jit_lock.unlockShared();
    return method != NULL;
}

void Profiler::recordSample(void* ucontext, u64 counter, jint event_type, jmethodID event) {
    u64 lock_index = atomicInc(_total_samples) % CONCURRENCY_LEVEL;
    if (!_locks[lock_index].tryLock()) {
        atomicInc(_failures[-ticks_skipped]);  // too many concurrent signals already
        return;
    }

    atomicInc(_total_counter, counter);

    ASGCT_CallFrame* frames = _calltrace_buffer[lock_index]._asgct_frames;
    int num_frames = makeEventFrame(frames, event_type, event);
    num_frames += getNativeTrace(ucontext, frames + num_frames);
    num_frames += getJavaTrace(ucontext, frames + num_frames, MAX_STACK_FRAMES - num_frames);

    if (num_frames > 0) {
        storeCallTrace(num_frames, frames, counter);
        storeMethod(frames[0].method_id, frames[0].bci, counter);
    }

    _locks[lock_index].unlock();
}

void Profiler::resetSymbols() {
    for (int i = 0; i < _native_lib_count; i++) {
        delete _native_libs[i];
    }
    _native_lib_count = Symbols::parseMaps(_native_libs, MAX_NATIVE_LIBS);
}

bool Profiler::start(Mode mode, int interval, int frame_buffer_size) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) return false;

    _total_samples = 0;
    _total_counter = 0;
    memset(_failures, 0, sizeof(_failures));
    memset(_hashes, 0, sizeof(_hashes));
    memset(_traces, 0, sizeof(_traces));
    memset(_methods, 0, sizeof(_methods));

    // Reset frames
    free(_frame_buffer);
    _frame_buffer_size = frame_buffer_size;
    _frame_buffer = (ASGCT_CallFrame*)malloc(_frame_buffer_size * sizeof(ASGCT_CallFrame));
    _frame_buffer_index = 0;
    _frame_buffer_overflow = false;

    resetSymbols();

    bool success;
    if (mode == MODE_CPU) {
        success = PerfEvents::start(interval);
    } else {
        success = AllocTracer::start();
    }

    if (!success) return false;

    _state = RUNNING;
    _mode = mode;
    _start_time = time(NULL);
    return true;
}

bool Profiler::stop() {
    MutexLocker ml(_state_lock);
    if (_state != RUNNING) return false;
    _state = IDLE;

    if (_mode == MODE_CPU) {
        PerfEvents::stop();
    } else {
        AllocTracer::stop();
    }
    return true;
}

void Profiler::dumpSummary(std::ostream& out) {
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

    char buf[256];
    snprintf(buf, sizeof(buf),
            "--- Execution profile ---\n"
            "Total samples:         %lld\n",
            _total_samples);
    out << buf;
    
    double percent = 100.0 / _total_samples;
    for (int i = 0; i < FAILURE_TYPES; i++) {
        if (_failures[i] > 0) {
            snprintf(buf, sizeof(buf), "%-22s %lld (%.2f%%)\n", title[i], _failures[i], _failures[i] * percent);
            out << buf;
        }
    }
    out << std::endl;

    if (_frame_buffer_overflow) {
        out << "Frame buffer overflowed! Consider increasing its size." << std::endl;
    } else {
        double usage = 100.0 * _frame_buffer_index / _frame_buffer_size;
        out << "Frame buffer usage:    " << usage << "%" << std::endl;
    }
    out << std::endl;
}

/*
 * Dump stacks in FlameGraph input format:
 * 
 * <frame>;<frame>;...;<topmost frame> <count>
 */
void Profiler::dumpCollapsed(std::ostream& out) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) return;

    for (int i = 0; i < MAX_CALLTRACES; i++) {
        CallTraceSample& trace = _traces[i];
        if (trace._counter == 0) continue;
        
        for (int j = trace._num_frames - 1; j >= 0; j--) {
            FrameName fn(_frame_buffer[trace._start_frame + j]);
            out << fn.toString() << (j == 0 ? ' ' : ';');
        }
        out << trace._counter << "\n";
    }
}

void Profiler::dumpTraces(std::ostream& out, int max_traces) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) return;

    double percent = 100.0 / _total_counter;
    char buf[1024];

    qsort(_traces, MAX_CALLTRACES, sizeof(CallTraceSample), CallTraceSample::comparator);
    if (max_traces > MAX_CALLTRACES) max_traces = MAX_CALLTRACES;

    for (int i = 0; i < max_traces; i++) {
        CallTraceSample& trace = _traces[i];
        if (trace._counter == 0) break;

        snprintf(buf, sizeof(buf), "Samples: %lld, Counter: %lld (%.2f%%)\n",
                 trace._samples, trace._counter, trace._counter * percent);
        out << buf;

        for (int j = 0; j < trace._num_frames; j++) {
            FrameName fn(_frame_buffer[trace._start_frame + j], true);
            snprintf(buf, sizeof(buf), "  [%2d] %s\n", j, fn.toString());
            out << buf;
        }
        out << "\n";
    }
}

void Profiler::dumpFlat(std::ostream& out, int max_methods) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) return;

    double percent = 100.0 / _total_counter;
    char buf[1024];

    qsort(_methods, MAX_CALLTRACES, sizeof(MethodSample), MethodSample::comparator);
    if (max_methods > MAX_CALLTRACES) max_methods = MAX_CALLTRACES;

    for (int i = 0; i < max_methods; i++) {
        u64 counter = _methods[i]._counter;
        if (counter == 0) break;

        FrameName fn(_methods[i]._method, true);
        snprintf(buf, sizeof(buf), "%10lld (%.2f%%) %s\n", counter, counter * percent, fn.toString());
        out << buf;
    }
}

void Profiler::runInternal(Arguments& args, std::ostream& out) {
    switch (args._action) {
        case ACTION_START:
            if (start(args._mode, args._interval, args._framebuf)) {
                out << mode() << " profiling started" << std::endl;
            } else {
                out << "Profiler failed to start" << std::endl;
            }
            break;
        case ACTION_STOP:
            if (stop()) {
                out << "Profiling stopped after " << uptime() << " seconds" << std::endl;
            } else {
                out << "Profiler is not active" << std::endl;
            }
            break;
        case ACTION_STATUS: {
            MutexLocker ml(_state_lock);
            if (_state == RUNNING) {
                out << mode() << " profiler is running for " << uptime() << " seconds" << std::endl;
            } else {
                out << "Profiler is not active" << std::endl;
            }
            break;
        }
        case ACTION_DUMP:
            stop();
            if (args._dump_collapsed) dumpCollapsed(out);
            if (args._dump_summary) dumpSummary(out);
            if (args._dump_traces > 0) dumpTraces(out, args._dump_traces);
            if (args._dump_flat > 0) dumpFlat(out, args._dump_flat);
            break;
    }
}

void Profiler::run(Arguments& args) {
    if (args._file == NULL) {
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
