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

#include <math.h>
#include <random>
#include <unistd.h>
#include "wallClock.h"
#include "profiler.h"
#include "stackFrame.h"
#include "context.h"

volatile bool WallClock::_enabled = false;

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

Error WallClock::start(Arguments &args) {
    if (_sample_idle_threads) {
        int interval = args._event != NULL ? args._interval : args._wall;
        if (interval < 0) {
            return Error("interval must be positive");
        }
        _interval = interval ? interval : DEFAULT_WALL_INTERVAL;

        _filtering = args._wall_filtering;

        _reservoir_size =
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

        _reservoir_size =
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

bool shouldSample(
        int self,
        int thread_id,
        bool sample_idle_threads,
        bool thread_filter_enabled,
        ThreadFilter* thread_filter) {
    return thread_id != -1 && thread_id != self && (!thread_filter_enabled || thread_filter->accept(thread_id))
           && (sample_idle_threads || OS::threadState(thread_id) == THREAD_RUNNING);
}

void WallClock::timerLoop() {
    std::vector<int> reservoir;
    reservoir.reserve(_reservoir_size);
    int self = OS::threadId();
    ThreadFilter* thread_filter = Profiler::instance()->threadFilter();
    bool thread_filter_enabled = thread_filter->enabled();
    bool sample_idle_threads = _sample_idle_threads;

    // FIXME: reenable when using thread filtering based on context. See context.cpp
    // ThreadList* thread_list = _filtering ? Contexts::listThreads() : OS::listThreads();
    ThreadList* thread_list = OS::listThreads();

    std::default_random_engine generator;
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    std::uniform_int_distribution<int> random_index(0, _reservoir_size);

    while (_running) {
        if (_enabled) {
            int num_threads = thread_list->size();
            for (int i = 0; i < num_threads && reservoir.size() < _reservoir_size; i++) {
                int thread_id = thread_list->next();
                if (shouldSample(self, thread_id, sample_idle_threads, thread_filter_enabled, thread_filter)) {
                    reservoir.push_back(thread_id);
                }
            }
            if (reservoir.size() == _reservoir_size) {
                double weight = exp(log(uniform(generator)) / _reservoir_size);
                for (int i = _reservoir_size, skip = 0; i < num_threads; i += skip) {
                    int thread_id = thread_list->next();
                    if (shouldSample(self, thread_id, sample_idle_threads, thread_filter_enabled, thread_filter)) {
                        reservoir[random_index(generator)] = thread_id;
                    }
                    skip = (int) (log(uniform(generator)) / log(1 - weight)) + 1;
                    weight *= exp(log(uniform(generator)) / _reservoir_size);
                    for (int j = 0; j < skip; j++) {
                        // FIXME - don't need to parse thread id, add API to skip next thread
                        thread_list->next();
                    }
                }
            }
            for (auto const &thread_id: reservoir) {
                OS::sendSignalToThread(thread_id, sample_idle_threads ? SIGVTALRM : SIGPROF);
            }
            reservoir.clear();
            thread_list->rewind();
        }
        OS::sleep(_interval);
    }
}
