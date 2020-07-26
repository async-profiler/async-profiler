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

#include "perfEvents.h"


int PerfEvents::_max_events;
PerfEvent* PerfEvents::_events;
PerfEventType* PerfEvents::_event_type;
long PerfEvents::_interval;
Ring PerfEvents::_ring;
bool PerfEvents::_print_extended_warning;


void PerfEvents::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
}

const char* PerfEvents::units() {
    return "ns";
}

Error PerfEvents::check(Arguments& args) {
    return Error("PerfEvents are unsupported on macOS");
}

Error PerfEvents::start(Arguments& args) {
    return Error("PerfEvents are unsupported on macOS");
}

void PerfEvents::stop() {
}

int PerfEvents::getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth,
                               CodeCache* java_methods, CodeCache* runtime_stubs) {
    return 0;
}

bool PerfEvents::supported() {
    return false;
}

const char* PerfEvents::getEventName(int event_id) {
    return NULL;
}

bool PerfEvents::createForThread(int tid) {
    return false;
}

void PerfEvents::destroyForThread(int tid) {
}

#endif // __APPLE__
