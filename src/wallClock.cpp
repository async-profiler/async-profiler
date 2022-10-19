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
    WallClock *engine = (WallClock*)Profiler::instance()->wallEngine();
    if (signo == SIGVTALRM) {
        engine->signalHandler(signo, siginfo, ucontext, engine->_interval);
    } else {
        Log::error("Unknown signal %d", signo);
    }
}

void WallClock::signalHandler(int signo, siginfo_t* siginfo, void* ucontext, u64 last_sample) {
    ProfiledThread* current = ProfiledThread::current();
    int tid = current != NULL ? current->tid() : OS::threadId();
    Context ctx = Contexts::get(tid);
    u64 skipped = 0;
    if (current != NULL) {
        if (_collapsing && !current->noteWallSample(false, &skipped)) {
            return;
        }
    }

    ExecutionEvent event;
    event._context = ctx;
    event._thread_state = getThreadState(ucontext);
    event._weight = skipped + 1;
    Profiler::instance()->recordSample(ucontext, last_sample, tid, BCI_WALL, &event);
}

Error WallClock::start(Arguments &args) {
    int interval = args._event != NULL ? args._interval : args._wall;
    if (interval < 0) {
        return Error("interval must be positive");
    }
    _interval = interval ? interval : DEFAULT_WALL_INTERVAL;

    _collapsing = args._wall_collapsing;

    _reservoir_size =
            args._wall_threads_per_tick ?
            args._wall_threads_per_tick :
            DEFAULT_WALL_THREADS_PER_TICK;

    OS::installSignalHandler(SIGVTALRM, sharedSignalHandler);

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
        bool thread_filter_enabled,
        ThreadFilter* thread_filter) {
    return thread_id != -1 && thread_id != self && (!thread_filter_enabled || thread_filter->accept(thread_id));
}

void WallClock::timerLoop() {
    std::vector<int> reservoir;
    reservoir.reserve(_reservoir_size);
    int self = OS::threadId();
    ThreadFilter* thread_filter = Profiler::instance()->threadFilter();
    bool thread_filter_enabled = thread_filter->enabled();

    ThreadList* thread_list = OS::listThreads();

    std::mt19937 generator(std::random_device{}());
    std::uniform_real_distribution<double> uniform(1e-16, 1.0);
    std::uniform_int_distribution<int> random_index(0, _reservoir_size - 1);

    while (_running) {
        if (_enabled) {
            int tid = thread_list->next();
            for (int i = 0; i < _reservoir_size && tid != -1; tid = thread_list->next()) {
                if (shouldSample(self, tid, thread_filter_enabled, thread_filter)) {
                    reservoir.push_back(tid);
                    i++;
                }
            }
            if (tid != -1) {
                double weight = exp(log(uniform(generator)) / _reservoir_size);
                // produces a value in [0, _)
                int num_threads_to_skip = (int) (log(uniform(generator)) / log(1 - weight));
                while (tid != -1) {
                    for (int i = 0; i < num_threads_to_skip && tid != -1;) {
                        tid = thread_list->next();
                        if (shouldSample(self, tid, thread_filter_enabled, thread_filter)) {
                            i++;
                        }
                    }
                    if (tid != -1) {
                        reservoir[random_index(generator)] = tid;
                    }
                    weight *= exp(log(uniform(generator)) / _reservoir_size);
                    num_threads_to_skip = (int) (log(uniform(generator)) / log(1 - weight));
                    tid = thread_list->next();
                }
            }
            for (auto const &thread_id: reservoir) {
                OS::sendSignalToThread(thread_id, SIGVTALRM);
            }
            reservoir.clear();
            thread_list->rewind();
        }
        OS::sleep(_interval);
    }
}
