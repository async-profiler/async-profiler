/*
 * Copyright 2021 Andrei Pangin
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

#include <signal.h>
#include <time.h>
#include "jstack.h"
#include "profiler.h"


// Wait at most this number of milliseconds to finish processing of pending signals
const int MAX_WAIT_MILLIS = 2000;


void JStack::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    ExecutionEvent event;
    event._thread_state = Profiler::_instance.getThreadState(ucontext);
    Profiler::_instance.recordSample(ucontext, 1, 0, &event);
}

Error JStack::start(Arguments& args) {
    OS::installSignalHandler(SIGVTALRM, signalHandler);

    int self = OS::threadId();
    u64 required_samples = Profiler::_instance.total_samples();
    ThreadFilter* thread_filter = Profiler::_instance.threadFilter();
    bool thread_filter_enabled = thread_filter->enabled();

    ThreadList* thread_list = OS::listThreads();
    int thread_id;
    while ((thread_id = thread_list->next()) != -1) {
        if (thread_id != self && (!thread_filter_enabled || thread_filter->accept(thread_id))) {
            if (OS::sendSignalToThread(thread_id, SIGVTALRM)) {
                required_samples++;
            }
        }
    }
    delete thread_list;

    // Get our own stack trace after all other threads
    if (!thread_filter_enabled || thread_filter->accept(self)) {
        ExecutionEvent event;
        event._thread_state = THREAD_RUNNING;
        Profiler::_instance.recordSample(NULL, 1, 0, &event);
        required_samples++;
    }

    // Wait until all asynchronous stack traces collected
    for (int i = 0; Profiler::_instance.total_samples() < required_samples && i < MAX_WAIT_MILLIS; i++) {
        struct timespec timeout = {0, 1000000};
        nanosleep(&timeout, NULL);
    }

    return Error::OK;
}

void JStack::stop() {
    // Nothing to do
}
