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
#include <stdlib.h>
#include <signal.h>
#include <poll.h>
#include <dirent.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include "wallClock.h"
#include "profiler.h"
#include "stackFrame.h"


const int THREADS_PER_TICK = 5;

long WallClock::_interval;

void WallClock::installSignalHandler() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = NULL;
    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;

    sigaction(SIGPROF, &sa, NULL);
}

void WallClock::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    Profiler::_instance.recordSample(ucontext, _interval, 0, NULL);
}

Error WallClock::start(const char* event, long interval) {
    if (interval < 0) {
        return Error("interval must be positive");
    }
    _interval = interval ? interval : DEFAULT_INTERVAL;

    installSignalHandler();

    _pid = getpid();
    _eventfd = eventfd(0, 0);
    if (_eventfd == -1) {
        return Error("Unable to create timer event");
    }

    if (pthread_create(&_thread, NULL, threadEntry, this) != 0) {
        close(_eventfd);
        return Error("Unable to create timer thread");
    }

    return Error::OK;
}

void WallClock::stop() {
    u64 val = 1;
    ssize_t r = write(_eventfd, &val, sizeof(val));
    (void)r;

    pthread_join(_thread, NULL);
}

void WallClock::timerLoop() {
    DIR* dir = NULL;

    struct timespec ts = {_interval / 1000000000, _interval % 1000000000};
    struct pollfd fds = {_eventfd, POLLIN, 0};

    while (ppoll(&fds, 1, &ts, NULL) == 0) {
        if (dir == NULL && (dir = opendir("/proc/self/task")) == NULL) {
            return;
        }

        for (int thread_count = 0; thread_count < THREADS_PER_TICK; ) {
            struct dirent* entry = readdir(dir);
            if (entry == NULL) {
                closedir(dir);
                dir = NULL;
                break;
            }

            if (entry->d_name[0] != '.') {
                int tid = atoi(entry->d_name);
                syscall(__NR_tgkill, _pid, tid, SIGPROF);
                thread_count++;
            }
        }
    }

    if (dir != NULL) {
        closedir(dir);
    }
}

int WallClock::getCallChain(void* ucontext, int tid, const void** callchain, int max_depth,
                            const void* jit_min_address, const void* jit_max_address) {
    StackFrame frame(ucontext);
    const void* pc = (const void*)frame.pc();
    uintptr_t fp = frame.fp();
    uintptr_t prev_fp = (uintptr_t)&fp;

    int depth = 0;
    const void* const valid_pc = (const void*)0x1000;

    // Walk until the bottom of the stack or until the first Java frame
    while (depth < max_depth && pc >= valid_pc && !(pc >= jit_min_address && pc < jit_max_address)) {
        callchain[depth++] = pc;

        // Check if the next frame is below on the current stack
        if (fp <= prev_fp || fp >= prev_fp + 0x40000) {
            break;
        }

        prev_fp = fp;
        pc = ((const void**)fp)[1];
        fp = ((uintptr_t*)fp)[0];
    }

    return depth;
}
