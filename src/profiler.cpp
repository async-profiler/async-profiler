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
#include "perfEvents.h"
#include "allocTracer.h"
#include "lockTracer.h"
#include "flameGraph.h"
#include "flightRecorder.h"
#include "frameName.h"
#include "stackFrame.h"
#include "symbols.h"
#include "vmStructs.h"


Profiler Profiler::_instance;


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

const void* Profiler::findSymbol(const char* name) {
    for (int i = 0; i < _native_lib_count; i++) {
        const void* address = _native_libs[i]->findSymbol(name);
        if (address != NULL) {
            return address;
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

int Profiler::getNativeTrace(void* ucontext, ASGCT_CallFrame* frames, int tid) {
    const void* native_callchain[MAX_NATIVE_FRAMES];
    int native_frames = PerfEvents::getCallChain(ucontext, tid, native_callchain, MAX_NATIVE_FRAMES,
                                                 _jit_min_address, _jit_max_address);

    for (int i = 0; i < native_frames; i++) {
        frames[i].bci = BCI_NATIVE_FRAME;
        frames[i].method_id = (jmethodID)findNativeMethod(native_callchain[i]);
    }

    return native_frames;
}

int Profiler::getJavaTraceAsync(void* ucontext, ASGCT_CallFrame* frames, int max_depth) {
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

        // Guess top method by PC and insert it manually into the call trace
        bool is_entry_frame = false;
        if (fillTopFrame((const void*)pc, trace.frames)) {
            is_entry_frame = trace.frames->bci == BCI_NATIVE_FRAME &&
                             strcmp((const char*)trace.frames->method_id, "call_stub") == 0;
            trace.frames++;
            max_depth--;
        }

        if (top_frame.pop(is_entry_frame)) {
            // Retry with the fixed context, but only if PC looks reasonable,
            // otherwise AsyncGetCallTrace may crash
            if (addressInCode((const void*)top_frame.pc())) {
                VM::_asyncGetCallTrace(&trace, max_depth, ucontext);
            }
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

int Profiler::getJavaTraceJvmti(jvmtiFrameInfo* jvmti_frames, ASGCT_CallFrame* frames, int max_depth) {
    // We cannot call pure JVM TI here, because it assumes _thread_in_native state,
    // but allocation events happen in _thread_in_vm state,
    // see https://github.com/jvm-profiling-tools/async-profiler/issues/64
    void* thread = _ThreadLocalStorage_thread();
    int num_frames;
    if (_JvmtiEnv_GetStackTrace(NULL, thread, 0, max_depth, jvmti_frames, &num_frames) == 0 && num_frames > 0) {
        // Profiler expects stack trace in AsyncGetCallTrace format; convert it now
        for (int i = 0; i < num_frames; i++) {
            frames[i].method_id = jvmti_frames[i].method;
            frames[i].bci = 0;
        }
        return num_frames;
    }

    atomicInc(_failures[-ticks_no_Java_frame]);
    return 0;
}

int Profiler::makeEventFrame(ASGCT_CallFrame* frames, jint event_type, jmethodID event) {
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

bool Profiler::addressInCode(const void* pc) {
    // 1. Check if PC lies within JVM's compiled code cache
    // Address in CodeCache is executable if it belongs to a Java method or a runtime stub
    if (pc >= _jit_min_address && pc < _jit_max_address) {
        _jit_lock.lockShared();
        bool valid = _java_methods.find(pc) != NULL || _runtime_stubs.find(pc) != NULL;
        _jit_lock.unlockShared();
        return valid;
    }

    // 2. Check if PC belongs to executable code of shared libraries
    for (int i = 0; i < _native_lib_count; i++) {
        if (_native_libs[i]->contains(pc)) {
            return true;
        }
    }
    
    // This can be some other dynamically generated code, but we don't know it. Better stay safe.
    return false;
}

void Profiler::recordSample(void* ucontext, u64 counter, jint event_type, jmethodID event) {
    u64 lock_index = atomicInc(_total_samples) % CONCURRENCY_LEVEL;
    if (!_locks[lock_index].tryLock()) {
        atomicInc(_failures[-ticks_skipped]);  // too many concurrent signals already
        return;
    }

    atomicInc(_total_counter, counter);

    ASGCT_CallFrame* frames = _calltrace_buffer[lock_index]._asgct_frames;
    int tid = PerfEvents::tid();

    int num_frames = 0;
    if (event_type == 0) {
        num_frames = getNativeTrace(ucontext, frames, tid);
    } else if (event != NULL) {
        num_frames = makeEventFrame(frames, event_type, event);
    }

    // It is possible to limit Java stack walking depth for performance reasons
    int max_depth = MAX_STACK_FRAMES - 1 - num_frames;
    if (_jstackdepth > 0 && _jstackdepth < max_depth) {
        max_depth = _jstackdepth;
    }

    if (event == NULL || _JvmtiEnv_GetStackTrace == NULL) {
        num_frames += getJavaTraceAsync(ucontext, frames + num_frames, max_depth);
    } else {
        // Events like object allocation happen at known places where it is safe to call JVM TI
        jvmtiFrameInfo* jvmti_frames = _calltrace_buffer[lock_index]._jvmti_frames;
        num_frames += getJavaTraceJvmti(jvmti_frames + num_frames, frames + num_frames, max_depth);
    }

    if (_threads) {
        num_frames += makeEventFrame(frames + num_frames, BCI_THREAD_ID, (jmethodID)(uintptr_t)tid);
    }

    if (num_frames > 0) {
        storeMethod(frames[0].method_id, frames[0].bci, counter);
        int call_trace_id = storeCallTrace(num_frames, frames, counter);
        _jfr.recordExecutionSample(lock_index, tid, call_trace_id);
    }

    _locks[lock_index].unlock();
}

void Profiler::initStateLock() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_state_lock, &attr);
}

void Profiler::resetSymbols() {
    for (int i = 0; i < _native_lib_count; i++) {
        delete _native_libs[i];
    }
    _native_lib_count = Symbols::parseMaps(_native_libs, MAX_NATIVE_LIBS);
}

void Profiler::initJvmtiFunctions(NativeCodeCache* libjvm) {
    if (_JvmtiEnv_GetStackTrace == NULL) {
        // Find ThreadLocalStorage::thread() if exists
        if (_ThreadLocalStorage_thread == NULL) {
            _ThreadLocalStorage_thread = (void* (*)()) libjvm->findSymbol("_ZN18ThreadLocalStorage6threadEv");
        }
        // Fallback to ThreadLocalStorage::get_thread_slow()
        if (_ThreadLocalStorage_thread == NULL) {
            _ThreadLocalStorage_thread = (void* (*)()) libjvm->findSymbol("_ZN18ThreadLocalStorage15get_thread_slowEv");
        }
        // JvmtiEnv::GetStackTrace(JavaThread* java_thread, jint start_depth, jint max_frame_count, jvmtiFrameInfo* frame_buffer, jint* count_ptr)
        if (_ThreadLocalStorage_thread != NULL) {
            _JvmtiEnv_GetStackTrace = (jvmtiError (*)(void*, void*, jint, jint, jvmtiFrameInfo*, jint*))
                libjvm->findSymbol("_ZN8JvmtiEnv13GetStackTraceEP10JavaThreadiiP15_jvmtiFrameInfoPi");
        }

        if (_JvmtiEnv_GetStackTrace == NULL) {
            fprintf(stderr, "WARNING: Install JVM debug symbols to improve profile accuracy\n");
        }
    }
}

Error Profiler::start(Arguments& args) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) {
        return Error("Profiler already started");
    }

    if (VM::_asyncGetCallTrace == NULL) {
        return Error("Could not find AsyncGetCallTrace function");
    }

    _total_samples = 0;
    _total_counter = 0;
    memset(_failures, 0, sizeof(_failures));
    memset(_hashes, 0, sizeof(_hashes));
    memset(_traces, 0, sizeof(_traces));
    memset(_methods, 0, sizeof(_methods));

    // Index 0 denotes special call trace with no frames
    _hashes[0] = (u64)-1;

    // Reset frames
    free(_frame_buffer);
    _jstackdepth = args._jstackdepth;
    _frame_buffer_size = args._framebuf;
    _frame_buffer = (ASGCT_CallFrame*)malloc(_frame_buffer_size * sizeof(ASGCT_CallFrame));
    _frame_buffer_index = 0;
    _frame_buffer_overflow = false;
    _threads = args._threads && !args._dump_jfr;

    resetSymbols();
    NativeCodeCache* libjvm = jvmLibrary();
    if (libjvm == NULL) {
        return Error("libjvm not found among loaded libraries");
    }
    VMStructs::init(libjvm);
    initJvmtiFunctions(libjvm);

    if (args._dump_jfr) {
        Error error = _jfr.start(args._file);
        if (error) {
            return error;
        }
    }

    if (strcmp(args._event, EVENT_CPU) == 0) {
        _engine = new PerfEvents(args);
        _units = "ns";
    } else if (strcmp(args._event, EVENT_ALLOC) == 0) {
        _engine = new AllocTracer();
        _units = "bytes";
    } else if (strcmp(args._event, EVENT_LOCK) == 0) {
        _engine = new LockTracer();
        _units = "ns";
    } else {
        _engine = new PerfEvents(args);
        _units = "events";
    }

    Error error = _engine->start(args._event, args._interval);
    if (error) {
        delete _engine;
        _jfr.stop();
        return error;
    }

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
    delete _engine;

    // Acquire all spinlocks to avoid race with remaining signals
    for (int i = 0; i < CONCURRENCY_LEVEL; i++) _locks[i].lock();
    _jfr.stop();
    for (int i = 0; i < CONCURRENCY_LEVEL; i++) _locks[i].unlock();

    _state = IDLE;
    return Error::OK;
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
void Profiler::dumpCollapsed(std::ostream& out, Arguments& args) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) return;

    FrameName fn(args._simple, false, _threads);
    u64 unknown = 0;

    for (int i = 0; i < MAX_CALLTRACES; i++) {
        CallTraceSample& trace = _traces[i];
        if (trace._samples == 0) continue;

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
    if (_state != IDLE) return;

    FlameGraph flamegraph(args._title, args._counter, args._width, args._height, args._minwidth, args._reverse);
    FrameName fn(args._simple, false, _threads);

    for (int i = 0; i < MAX_CALLTRACES; i++) {
        CallTraceSample& trace = _traces[i];
        if (trace._samples == 0) continue;

        u64 samples = (args._counter == COUNTER_SAMPLES ? trace._samples : trace._counter);

        Trie* f = flamegraph.root();
        if (trace._num_frames == 0) {
            f = f->addChild("[frame_buffer_overflow]", samples);
        } else if (args._reverse) {
            for (int j = 0; j < trace._num_frames; j++) {
                const char* frame_name = fn.name(_frame_buffer[trace._start_frame + j]);
                f = f->addChild(frame_name, samples);
            }
        } else {
            for (int j = trace._num_frames - 1; j >= 0; j--) {
                const char* frame_name = fn.name(_frame_buffer[trace._start_frame + j]);
                f = f->addChild(frame_name, samples);
            }
        }
        f->addLeaf(samples);
    }

    flamegraph.dump(out, tree);
}

void Profiler::dumpTraces(std::ostream& out, int max_traces) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) return;

    FrameName fn(false, true, _threads);
    double percent = 100.0 / _total_counter;
    char buf[1024];

    qsort(_traces, MAX_CALLTRACES, sizeof(CallTraceSample), CallTraceSample::comparator);
    if (max_traces > MAX_CALLTRACES) max_traces = MAX_CALLTRACES;

    for (int i = 0; i < max_traces; i++) {
        CallTraceSample& trace = _traces[i];
        if (trace._samples == 0) break;

        snprintf(buf, sizeof(buf), "--- %lld %s (%.2f%%), %lld sample%s\n",
                 trace._counter, _units, trace._counter * percent,
                 trace._samples, trace._samples == 1 ? "" : "s");
        out << buf;

        if (trace._num_frames == 0) {
            out << "  [ 0] [frame_buffer_overflow]\n";
        }

        for (int j = 0; j < trace._num_frames; j++) {
            const char* frame_name = fn.name(_frame_buffer[trace._start_frame + j]);
            snprintf(buf, sizeof(buf), "  [%2d] %s\n", j, frame_name);
            out << buf;
        }
        out << "\n";
    }
}

void Profiler::dumpFlat(std::ostream& out, int max_methods) {
    MutexLocker ml(_state_lock);
    if (_state != IDLE) return;

    FrameName fn(false, true, _threads);
    double percent = 100.0 / _total_counter;
    char buf[1024];

    qsort(_methods, MAX_CALLTRACES, sizeof(MethodSample), MethodSample::comparator);
    if (max_methods > MAX_CALLTRACES) max_methods = MAX_CALLTRACES;

    snprintf(buf, sizeof(buf), "%12s  percent  samples  top\n"
                               "  ----------  -------  -------  ---\n", _units);
    out << buf;

    for (int i = 0; i < max_methods; i++) {
        MethodSample& method = _methods[i];
        if (method._samples == 0) break;

        const char* frame_name = fn.name(method._method);
        snprintf(buf, sizeof(buf), "%12lld  %6.2f%%  %7lld  %s\n",
                 method._counter, method._counter * percent, method._samples, frame_name);
        out << buf;
    }
}

void Profiler::runInternal(Arguments& args, std::ostream& out) {
    switch (args._action) {
        case ACTION_START: {
            Error error = start(args);
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
            out << "Perf events:" << std::endl;
            const char** perf_events = PerfEvents::getAvailableEvents();
            for (const char** event = perf_events; *event != NULL; event++) {
                out << "  " << *event << std::endl;
            }
            delete[] perf_events;

            out << "Java events:" << std::endl;
            out << "  " << EVENT_ALLOC << std::endl;
            out << "  " << EVENT_LOCK << std::endl;
            break;
        }
        case ACTION_VERSION:
            out << FULL_VERSION_STRING;
            break;
        case ACTION_DUMP:
            stop();
            if (args._dump_collapsed) dumpCollapsed(out, args);
            if (args._dump_flamegraph) dumpFlameGraph(out, args, false);
            if (args._dump_tree) dumpFlameGraph(out, args, true);
            if (args._dump_summary) dumpSummary(out);
            if (args._dump_traces > 0) dumpTraces(out, args._dump_traces);
            if (args._dump_flat > 0) dumpFlat(out, args._dump_flat);
            break;
        default:
            break;
    }
}

void Profiler::run(Arguments& args) {
    if (args._file == NULL || args._dump_jfr) {
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
    if (_state == RUNNING && args.dumpRequested()) {
        args._action = ACTION_DUMP;
        run(args);
    }

    _state = TERMINATED;
}
