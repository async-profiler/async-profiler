/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/time.h>
#include "itimer.h"
#include "j9StackTraces.h"
#include "log.h"
#include "os.h"
#include "vmEntry.h"


Error ITimer::start(Arguments& args) {
    if (args._interval < 0) {
        return Error("interval must be positive");
    }
    _interval = args._interval ? args._interval : DEFAULT_INTERVAL;
    _cstack = args._cstack;
    _signal = SIGPROF;
    _count_overrun = false;

    if (VM::isOpenJ9()) {
        OS::installSignalHandler(SIGPROF, signalHandlerJ9);
        Error error = J9StackTraces::start(args);
        if (error) {
            return error;
        }
    } else {
        OS::installSignalHandler(SIGPROF, signalHandler);
    }

    time_t sec = _interval / 1000000000;
    suseconds_t usec = (_interval % 1000000000) / 1000;
    struct itimerval tv = {{sec, usec}, {sec, usec}};

    if (setitimer(ITIMER_PROF, &tv, NULL) != 0) {
        return Error("ITIMER_PROF is not supported on this system");
    }

    return Error::OK;
}

void ITimer::stop() {
    struct itimerval tv = {{0, 0}, {0, 0}};
    setitimer(ITIMER_PROF, &tv, NULL);

    J9StackTraces::stop();
}
