/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "wallClock.h"
#include "profiler.h"
#include "stackFrame.h"
#include "tsc.h"


// Maximum number of threads sampled in one iteration. This limit serves as a throttle
// when generating profiling signals. Otherwise applications with too many threads may
// suffer from a big profiling overhead. Also, keeping this limit low enough helps
// to avoid contention on a spin lock inside Profiler::recordSample().
const int THREADS_PER_TICK = 8;

// Set the hard limit for thread walking interval to 100 microseconds.
// Smaller intervals are practically unusable due to large overhead.
const long long MIN_INTERVAL = 100000;

// How much CPU time a thread can spend after being sampled as idle
// until it is considered runnable.
const u64 RUNNABLE_THRESHOLD_NS = 10000;

// How many skipped idle samples can be recorded in a single WallClock event.
const u32 MAX_IDLE_BATCH = 1000;


struct ThreadSleepState {
    u64 start_time;
    u64 last_cpu_time;
    u32 call_trace_id;
    u32 counter;
};

typedef std::map<int, ThreadSleepState> ThreadSleepMap;

struct ThreadCpuTime {
    u64 cpu_time;
    u64 trace;
};

// MPSC ring buffer
class ThreadCpuTimeBuffer {
  private:
    enum {
        RINGBUF_SIZE = 256,
        PAD_SIZE = 128
    };

    char _pad0[PAD_SIZE];  // protection against false sharing
    volatile u32 _write_ptr;
    char _pad1[PAD_SIZE - sizeof(u32)];
    u32 _read_ptr;
    char _pad2[PAD_SIZE - sizeof(u32)];
    ThreadCpuTime _ringbuf[RINGBUF_SIZE];

  public:
    ThreadCpuTimeBuffer() : _ringbuf(), _write_ptr(0), _read_ptr(0) {
    }

    void add(u64 trace) {
        ThreadCpuTime& t = _ringbuf[atomicInc(_write_ptr) & (RINGBUF_SIZE - 1)];
        t.trace = trace;
        storeRelease(t.cpu_time, OS::threadCpuTime(0));
    }

    void drain(ThreadSleepMap& thread_sleep_state) {
        u64 read_limit = _read_ptr + RINGBUF_SIZE;
        do {
            ThreadCpuTime& t = _ringbuf[_read_ptr & (RINGBUF_SIZE - 1)];
            u64 cpu_time = loadAcquire(t.cpu_time);
            if (cpu_time == 0) {
                break;
            }

            u64 trace = t.trace;
            if (__sync_bool_compare_and_swap(&t.cpu_time, cpu_time, 0)) {
                int thread_id = trace >> 32;
                ThreadSleepState& tss = thread_sleep_state[thread_id];
                tss.last_cpu_time = cpu_time;
                tss.call_trace_id = (u32)trace;
                tss.counter = 0;
                _read_ptr++;
            }
        } while (_read_ptr < read_limit);
    }
};

static ThreadCpuTimeBuffer _thread_cpu_time_buf;


long WallClock::_interval;
int WallClock::_signal;
WallClock::Mode WallClock::_mode;

ThreadState WallClock::getThreadState(void* ucontext) {
    StackFrame frame(ucontext);
    uintptr_t pc = frame.pc();

    // Consider a thread sleeping, if it has been interrupted in the middle of syscall execution,
    // either when PC points to the syscall instruction, or if syscall has just returned with EINTR
    if (StackFrame::isSyscall((instruction_t*)pc)) {
        return THREAD_SLEEPING;
    }

    // Make sure the previous instruction address is readable
    uintptr_t prev_pc = pc - SYSCALL_SIZE;
    if ((pc & 0xfff) >= SYSCALL_SIZE || Profiler::instance()->findLibraryByAddress((instruction_t*)prev_pc) != NULL) {
        if (StackFrame::isSyscall((instruction_t*)prev_pc) && frame.checkInterruptedSyscall()) {
            return THREAD_SLEEPING;
        }
    }

    return THREAD_RUNNING;
}

void WallClock::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (_mode == WALL_BATCH) {
        WallClockEvent event;
        event._start_time = TSC::ticks();
        event._thread_state = getThreadState(ucontext);
        event._samples = 1;
        u64 trace = Profiler::instance()->recordSample(ucontext, _interval, WALL_CLOCK_SAMPLE, &event);
        if (event._thread_state == THREAD_SLEEPING) {
            _thread_cpu_time_buf.add(trace);
        }
    } else {
        ExecutionEvent event(TSC::ticks());
        event._thread_state = _mode == CPU_ONLY ? THREAD_UNKNOWN : getThreadState(ucontext);
        Profiler::instance()->recordSample(ucontext, _interval, EXECUTION_SAMPLE, &event);
    }
}

void WallClock::recordWallClock(u64 start_time, ThreadState state, u32 samples, int tid, u32 call_trace_id) {
    WallClockEvent event;
    event._start_time = start_time;
    event._thread_state = state;
    event._samples = samples;
    Profiler::instance()->recordExternalSamples(samples, samples * _interval, tid, call_trace_id, WALL_CLOCK_SAMPLE, &event);
}

Error WallClock::start(Arguments& args) {
    if (args._wall >= 0 || strcmp(args._event, EVENT_WALL) == 0) {
        _mode = args._nobatch ? WALL_LEGACY : WALL_BATCH;
    } else {
        _mode = CPU_ONLY;
    }

    _interval = args._wall >= 0 ? args._wall : args._interval;
    if (_interval == 0) {
        // Increase default interval for wall clock mode due to larger number of sampled threads
        _interval = _mode == CPU_ONLY ? DEFAULT_INTERVAL : DEFAULT_INTERVAL * 5;
    }

    _signal = args._signal == 0 ? OS::getProfilingSignal(1)
                                : ((args._signal >> 8) > 0 ? args._signal >> 8 : args._signal);
    OS::installSignalHandler(_signal, signalHandler);

    _running = true;

    if (pthread_create(&_thread, NULL, threadEntry, this) != 0) {
        return Error("Unable to create timer thread");
    }

    return Error::OK;
}

void WallClock::stop() {
    _running = false;
    pthread_kill(_thread, WAKEUP_SIGNAL);
    pthread_join(_thread, NULL);
}

void WallClock::timerLoop() {
    int self = OS::threadId();
    ThreadFilter* thread_filter = Profiler::instance()->threadFilter();
    bool thread_filter_enabled = thread_filter->enabled();
    Mode mode = _mode;

    ThreadSleepMap thread_sleep_state;
    ThreadList* thread_list = OS::listThreads();
    u64 cycle_start_time = OS::nanotime();

    while (_running) {
        bool enabled = _enabled;

        for (int signaled_threads = 0; signaled_threads < THREADS_PER_TICK && thread_list->hasNext(); ) {
            int thread_id = thread_list->next();
            if (thread_id == self || (thread_filter_enabled && !thread_filter->accept(thread_id))) {
                continue;
            }

            if (mode == CPU_ONLY) {
                if (!enabled || OS::threadState(thread_id) == THREAD_SLEEPING) {
                    continue;
                }
            } else if (mode == WALL_BATCH) {
                ThreadSleepState& tss = thread_sleep_state[thread_id];
                u64 new_thread_cpu_time = enabled ? OS::threadCpuTime(thread_id) : 0;
                if (new_thread_cpu_time != 0 && new_thread_cpu_time - tss.last_cpu_time <= RUNNABLE_THRESHOLD_NS) {
                    if (++tss.counter < MAX_IDLE_BATCH) {
                        if (tss.counter == 1) tss.start_time = TSC::ticks();
                        continue;
                    }
                }
                if (tss.counter != 0) {
                    recordWallClock(tss.start_time, THREAD_SLEEPING, tss.counter, thread_id, tss.call_trace_id);
                    tss.counter = 0;
                }
            }

            if (enabled && OS::sendSignalToThread(thread_id, _signal)) {
                signaled_threads++;
            }
        }

        u64 current_time = OS::nanotime();
        if (thread_list->hasNext()) {
            // Try to keep interval stable regardless of the number of profiled threads
            long long sleep_time = cycle_start_time + (u64)_interval * thread_list->index() / thread_list->count() - current_time;
            OS::sleep(sleep_time < MIN_INTERVAL ? MIN_INTERVAL : sleep_time);
        } else {
            // Cycle has ended: prepare for the next cycle
            cycle_start_time += (u64)_interval;
            long long sleep_time = cycle_start_time - current_time;
            if (sleep_time < MIN_INTERVAL) {
                cycle_start_time = current_time + MIN_INTERVAL;
                sleep_time = MIN_INTERVAL;
            }
            OS::sleep(sleep_time);
            thread_list->update();
        }

        // Sync thread CPU times updated since the previous iteration
        _thread_cpu_time_buf.drain(thread_sleep_state);
    }

    delete thread_list;

    // Flush remaining WallClock batches
    for (ThreadSleepMap::const_iterator it = thread_sleep_state.begin(); it != thread_sleep_state.end(); ++it) {
        const ThreadSleepState& tss = it->second;
        if (tss.counter != 0) {
            recordWallClock(tss.start_time, THREAD_SLEEPING, tss.counter, it->first, tss.call_trace_id);
        }
    }
}
