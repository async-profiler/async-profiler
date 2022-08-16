/*
 * Copyright 2018 Andrei Pangin
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

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "wallClock.h"
#include "profiler.h"
#include "stackFrame.h"
#include "context.h"

// Set the hard limit for thread walking interval to 100 microseconds.
// Smaller intervals are practically unusable due to large overhead.
const long MIN_INTERVAL = 100000;

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

void WallClock::sharedSignalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    WallClock *engine;
    switch (signo) {
    case SIGPROF:
        engine = (WallClock*)Profiler::instance()->cpuEngine();
        break;
    case SIGVTALRM:
        engine = (WallClock*)Profiler::instance()->wallEngine();
        break;
    default:
        Log::error("Unknown signal %d", signo);
        return;
    }
    engine->signalHandler(signo, siginfo, ucontext, engine->_interval);
}

void WallClock::signalHandler(int signo, siginfo_t* siginfo, void* ucontext, u64 last_sample) {
    int tid = OS::threadId();
    int event_type = _sample_idle_threads ? BCI_WALL : BCI_CPU;
    if (!Contexts::filter(tid, event_type)) {
        return;
    }

    ExecutionEvent event;
    event._thread_state = _sample_idle_threads ? getThreadState(ucontext) : THREAD_RUNNING;
    Profiler::instance()->recordSample(ucontext, last_sample, tid, event_type, &event);
}

long WallClock::adjustInterval(long interval, int thread_count) {
    if (thread_count > _threads_per_tick) {
        interval /= (thread_count + _threads_per_tick - 1) / _threads_per_tick;
    }
    return interval;
}

Error WallClock::start(Arguments& args) {
    if (_sample_idle_threads) {
        int interval = args._event != NULL ? args._interval : args._wall;
        if (interval < 0) {
            return Error("interval must be positive");
        }
        _interval = interval ? interval : DEFAULT_WALL_INTERVAL;

        _filtering = args._wall_filtering;

        _threads_per_tick =
            args._wall_threads_per_tick ?
                args._wall_threads_per_tick :
                DEFAULT_WALL_THREADS_PER_TICK;

        OS::installSignalHandler(SIGVTALRM, sharedSignalHandler);
    } else {
        int interval = args._event != NULL ? args._interval : args._cpu;
        if (interval < 0) {
            return Error("interval must be positive");
        }
        _interval = interval ? interval : DEFAULT_CPU_INTERVAL;

        _filtering = args._cpu_filtering;

        _threads_per_tick =
            args._cpu_threads_per_tick ?
                args._cpu_threads_per_tick :
                DEFAULT_CPU_THREADS_PER_TICK;

        OS::installSignalHandler(SIGPROF, sharedSignalHandler);
    }

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
    bool sample_idle_threads = _sample_idle_threads;

    // FIXME: reenable when using thread filtering based on context. See context.cpp
    // ThreadList* thread_list = _filtering ? Contexts::listThreads() : OS::listThreads();
    ThreadList* thread_list = OS::listThreads();
    long long next_cycle_time = OS::nanotime();

    while (_running) {
        if (!_enabled) {
            OS::sleep(_interval);
            continue;
        }

        if (sample_idle_threads) {
            // Try to keep the wall clock interval stable, regardless of the number of profiled threads
            int estimated_thread_count = thread_filter_enabled ? thread_filter->size() : thread_list->size();
            next_cycle_time += adjustInterval(_interval, estimated_thread_count);
        }

        for (int count = 0; count < _threads_per_tick; ) {
            int thread_id = thread_list->next();
            if (thread_id == -1) {
                thread_list->rewind();
                break;
            }

            if (thread_id == self || (thread_filter_enabled && !thread_filter->accept(thread_id))) {
                continue;
            }

            if (sample_idle_threads || OS::threadState(thread_id) == THREAD_RUNNING) {
                if (OS::sendSignalToThread(thread_id, sample_idle_threads ? SIGVTALRM : SIGPROF)) {
                    count++;
                }
            }
        }

        if (sample_idle_threads) {
            long long current_time = OS::nanotime();
            if (next_cycle_time - current_time > MIN_INTERVAL) {
                OS::sleep(next_cycle_time - current_time);
            } else {
                next_cycle_time = current_time + MIN_INTERVAL;
                OS::sleep(MIN_INTERVAL);
            }
        } else {
            OS::sleep(_interval);
        }
    }

    delete thread_list;
}
