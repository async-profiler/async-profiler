/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include "ctimer.h"
#include "j9StackTraces.h"
#include "profiler.h"
#include "stackWalker.h"


#ifndef SIGEV_THREAD_ID
#define SIGEV_THREAD_ID  4
#endif


static inline clockid_t thread_cpu_clock(unsigned int tid) {
    return ((~tid) << 3) | 6;  // CPUCLOCK_SCHED | CPUCLOCK_PERTHREAD_MASK
}


int CTimer::_max_timers = 0;
int* CTimer::_timers = NULL;

int CTimer::createForThread(int tid) {
    if (tid >= _max_timers) {
        Log::warn("tid[%d] > pid_max[%d]. Restart profiler after changing pid_max", tid, _max_timers);
        return -1;
    }

    struct sigevent sev;
    sev.sigev_value.sival_ptr = NULL;
    sev.sigev_signo = _signal;
    sev.sigev_notify = SIGEV_THREAD_ID;
    ((int*)&sev.sigev_notify)[1] = tid;

    // Use raw syscalls, since libc wrapper allows only predefined clocks
    clockid_t clock = thread_cpu_clock(tid);
    int timer;
    if (syscall(__NR_timer_create, clock, &sev, &timer) < 0) {
        return -1;
    }

    // Kernel timer ID may start with zero, but we use zero as an empty slot
    if (!__sync_bool_compare_and_swap(&_timers[tid], 0, timer + 1)) {
        // Lost race
        syscall(__NR_timer_delete, timer);
        return -1;
    }

    struct itimerspec ts;
    ts.it_interval.tv_sec = (time_t)(_interval / 1000000000);
    ts.it_interval.tv_nsec = _interval % 1000000000;
    ts.it_value = ts.it_interval;
    syscall(__NR_timer_settime, timer, 0, &ts, NULL);
    return 0;
}

void CTimer::destroyForThread(int tid) {
    if (tid >= _max_timers) {
        return;
    }

    int timer = _timers[tid];
    if (timer != 0 && __sync_bool_compare_and_swap(&_timers[tid], timer--, 0)) {
        syscall(__NR_timer_delete, timer);
    }
}

Error CTimer::check(Arguments& args) {
    if (!setupThreadHook()) {
        return Error("Could not set pthread hook");
    }

    timer_t timer;
    if (timer_create(CLOCK_THREAD_CPUTIME_ID, NULL, &timer) < 0) {
        return Error("Failed to create CPU timer");
    }
    timer_delete(timer);

    return Error::OK;
}

Error CTimer::start(Arguments& args) {
    if (!setupThreadHook()) {
        return Error("Could not set pthread hook");
    }

    if (args._interval < 0) {
        return Error("interval must be positive");
    }
    _interval = args._interval ? args._interval : DEFAULT_INTERVAL;
    _cstack = args._cstack;
    _signal = args._signal == 0 ? OS::getProfilingSignal(0) : args._signal & 0xff;

    int max_timers = OS::getMaxThreadId();
    if (max_timers != _max_timers) {
        free(_timers);
        _timers = (int*)calloc(max_timers, sizeof(int));
        _max_timers = max_timers;
    }

    if (VM::isOpenJ9()) {
        if (_cstack == CSTACK_DEFAULT) _cstack = CSTACK_DWARF;
        OS::installSignalHandler(_signal, signalHandlerJ9);
        Error error = J9StackTraces::start(args);
        if (error) {
            return error;
        }
    } else {
        OS::installSignalHandler(_signal, signalHandler);
    }

    // Enable pthread hook before traversing currently running threads
    enableThreadHook();

    // Create timers for all existing threads
    int err = createForAllThreads();
    if (err) {
        disableThreadHook();
        J9StackTraces::stop();
        return Error("Failed to create CPU timer");
    }
    return Error::OK;
}

void CTimer::stop() {
    disableThreadHook();
    for (int i = 0; i < _max_timers; i++) {
        destroyForThread(i);
    }
    J9StackTraces::stop();
}

#endif // __linux__
