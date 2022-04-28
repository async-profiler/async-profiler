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

#ifndef _PERFEVENTS_H
#define _PERFEVENTS_H

#include <signal.h>
#include "arch.h"
#include "engine.h"


class PerfEvent;
class PerfEventType;
class StackContext;

class PerfEvents : public Engine {
  private:
    static int _max_events;
    static PerfEvent* _events;
    static PerfEventType* _event_type;
    static long _interval;
    static Ring _ring;
    static CStack _cstack;
    static bool _use_mmap_page;

    static u64 readCounter(siginfo_t* siginfo, void* ucontext);
    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);
    static void signalHandlerJ9(int signo, siginfo_t* siginfo, void* ucontext);

  public:
    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    const char* title();
    const char* units();

    static int walk(int tid, void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx);
    static void resetBuffer(int tid);

    static bool supported();
    static const char* getEventName(int event_id);

    static int createForThread(int tid);
    static void destroyForThread(int tid);
};

#endif // _PERFEVENTS_H
