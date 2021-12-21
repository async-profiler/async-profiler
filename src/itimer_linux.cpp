/*
 * Copyright (c) 2021, Datadog, Inc. All rights reserved.
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

#ifdef __linux__

#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "itimer.h"
#include "os.h"
#include "profiler.h"


static timer_t *_timerids = NULL;
static int _timerids_sz = -1;

long ITimer::_interval;

void ITimer::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (!_enabled) return;

    ExecutionEvent event;
    Profiler::instance()->recordSample(ucontext, _interval, 0, &event);
}

Error ITimer::check(Arguments& args) {
    OS::installSignalHandler(SIGPROF, NULL, SIG_IGN);
    return Error::OK;
}

Error ITimer::start(Arguments& args) {
    if (args._interval < 0) {
        return Error("interval must be positive");
    }
    _interval = args._interval ? args._interval : DEFAULT_INTERVAL;

    int timerids_sz = OS::getMaxThreadId();
    if (timerids_sz != _timerids_sz) {
        _timerids = (timer_t*)realloc(_timerids, (_timerids_sz = timerids_sz) * sizeof(timer_t));
    }

    OS::installSignalHandler(SIGPROF, signalHandler);

    // Enable thread events before traversing currently running threads
    Profiler::instance()->switchThreadEvents(JVMTI_ENABLE);

    // Create timers for all existing threads
    int err;
    bool created = false;
    ThreadList* thread_list = OS::listThreads();
    for (int tid; (tid = thread_list->next()) != -1; ) {
        if ((err = registerThread(tid)) == 0) {
            created = true;
        }
    }
    delete thread_list;

    if (!created) {
        Profiler::instance()->switchThreadEvents(JVMTI_DISABLE);
        if (err == EACCES || err == EPERM) {
            return Error("No access to perf events. Try --fdtransfer or --all-user option or 'sysctl kernel.perf_event_paranoid=1'");
        } else {
            return Error("Perf events unavailable");
        }
    }
    return Error::OK;
}

void ITimer::stop() {
    for (int i = 0; i < _timerids_sz; i++) {
        unregisterThread(i);
    }
}

// Defined in /usr/include/asm-generic/siginfo.h
#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id _sigev_un._tid
#endif // sigev_notify_thread_id

int ITimer::registerThread(int tid) {
    struct sigevent sev = { 0 };
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = SIGPROF;
    sev.sigev_notify_thread_id = tid;
    if (timer_create(CLOCK_THREAD_CPUTIME_ID, &sev, &_timerids[tid]) != 0) {
        int err = errno;
        Log::warn("timer_create for TID %d failed: %s", tid, strerror(errno));
        return err;
    }

    time_t sec = _interval / (1000 * 1000 * 1000);
    long nsec = _interval % (1000 * 1000 * 1000);
    struct itimerspec nval = { { sec, nsec }, { sec, nsec } };
    if (timer_settime(_timerids[tid], 0, &nval, NULL) != 0) {
        int err = errno;
        Log::warn("timer_settime for TID %d failed: %s", tid, strerror(errno));
        timer_delete(_timerids[tid]);
        _timerids[tid] = 0;
        return err;
    }

    return 0;
}

void ITimer::unregisterThread(int tid) {
    if (_timerids[tid] != 0) {
        timer_delete(_timerids[tid]);
        _timerids[tid] = 0;
    }
}

#endif // __linux__
