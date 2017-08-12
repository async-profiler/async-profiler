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

#include <jvmti.h>
#include <signal.h>


class PerfEvent;

class PerfEvents {
  private:
    static int _max_events;
    static PerfEvent* _events;
    static int _interval;

    static int tid();
    static int getMaxPid();
    static void createForThread(int tid);
    static void createForAllThreads();
    static void destroyForThread(int tid);
    static void destroyForAllThreads();
    static void installSignalHandler();
    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);

  public:
    static void init();
    static bool start(int interval);
    static void stop();
    static int getCallChain(const void** callchain, int max_depth);

    static void JNICALL ThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
    static void JNICALL ThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
};

#endif // _PERFEVENTS_H
