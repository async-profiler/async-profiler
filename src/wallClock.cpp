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

#include <poll.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include "wallClock.h"
#include "os.h"
#include "profiler.h"


const int THREADS_PER_TICK = 8;

long WallClock::_interval;
bool WallClock::_sample_idle_threads;

void WallClock::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    Profiler::_instance.recordSample(ucontext, _interval, 0, NULL);
}

Error WallClock::start(Arguments& args) {
    if (args._interval < 0) {
        return Error("interval must be positive");
    }
    _interval = args._interval ? args._interval : DEFAULT_INTERVAL;
    _sample_idle_threads = strcmp(args._event, EVENT_WALL) == 0;

    OS::installSignalHandler(SIGPROF, signalHandler);

    if (pipe(_pipefd) != 0) {
        return Error("Unable to create poll pipe");
    }

    if (pthread_create(&_thread, NULL, threadEntry, this) != 0) {
        close(_pipefd[1]);
        close(_pipefd[0]);
        return Error("Unable to create timer thread");
    }

    return Error::OK;
}

void WallClock::stop() {
    char val = 1;
    ssize_t r = write(_pipefd[1], &val, sizeof(val));
    (void)r;

    close(_pipefd[1]);
    pthread_join(_thread, NULL);
    close(_pipefd[0]);
}

void WallClock::timerLoop() {
    ThreadList* thread_list = NULL;

    int self = OS::threadId();
    bool sample_idle_threads = _sample_idle_threads;
    struct pollfd fds = {_pipefd[0], POLLIN, 0};
    int timeout = _interval > 1000000 ? (int)(_interval / 1000000) : 1;

    while (poll(&fds, 1, timeout) == 0) {
        if (thread_list == NULL) {
            thread_list = OS::listThreads();
        }

        for (int count = 0; count < THREADS_PER_TICK; ) {
            int thread_id = thread_list->next();
            if (thread_id == -1) {
                delete thread_list;
                thread_list = NULL;
                break;
            }
            if (thread_id != self && (sample_idle_threads || OS::isThreadRunning(thread_id))) {
                OS::sendSignalToThread(thread_id, SIGPROF);
                count++;
            }
        }
    }

    delete thread_list;
}
