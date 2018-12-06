/*
 * Copyright 2017 Andrei Pangin
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

#ifdef __APPLE__

#include <string.h>
#include <sys/time.h>
#include "perfEvents.h"
#include "profiler.h"


int PerfEvents::_max_events;
PerfEvent* PerfEvents::_events;
PerfEventType* PerfEvents::_event_type;
long PerfEvents::_interval;
Ring PerfEvents::_ring;
bool PerfEvents::_print_extended_warning;


bool PerfEvents::createForThread(int tid)  { return false; }
bool PerfEvents::createForAllThreads()     { return false; }
void PerfEvents::destroyForThread(int tid) {}
void PerfEvents::destroyForAllThreads()    {}


void PerfEvents::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    Profiler::_instance.recordSample(ucontext, _interval, 0, NULL);
}

const char* PerfEvents::units() {
    return "ns";
}

Error PerfEvents::start(Arguments& args) {
    if (strcmp(args._event, EVENT_CPU) != 0) {
        return Error("Event is not supported on this platform");
    }

    if (args._interval < 0) {
        return Error("interval must be positive");
    }
    _interval = args._interval ? args._interval : DEFAULT_INTERVAL;

    OS::installSignalHandler(SIGPROF, signalHandler);

    long sec = _interval / 1000000000;
    long usec = (_interval % 1000000000) / 1000;
    struct itimerval tv = {{sec, usec}, {sec, usec}};
    setitimer(ITIMER_PROF, &tv, NULL);

    return Error::OK;
}

void PerfEvents::stop() {
    struct itimerval tv = {{0, 0}, {0, 0}};
    setitimer(ITIMER_PROF, &tv, NULL);
}

int PerfEvents::getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth,
                               const void* jit_min_address, const void* jit_max_address) {
    return Engine::getNativeTrace(ucontext, tid, callchain, max_depth, jit_min_address, jit_max_address);
}

const char** PerfEvents::getAvailableEvents() {
    const char** available_events = new const char*[2];
    available_events[0] = "cpu";
    available_events[1] = NULL;
    return available_events;
}

#endif // __APPLE__
