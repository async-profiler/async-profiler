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

#ifndef _WALLCLOCK_H
#define _WALLCLOCK_H

#include <vector>
#include <jvmti.h>
#include <signal.h>
#include <pthread.h>
#include "engine.h"
#include "mutex.h"
#include <jni.h>

typedef void (JNICALL *ThreadSetNameFunc)(JNIEnv*, jobject, jstring);

class WallClock : public Engine {
  private:
    static long _interval;
    static bool _sample_idle_threads;

    int _pipefd[2];
    pthread_t _thread;

    void timerLoop();
    void filteredTimerLoop();
    char* _filter_threads;

    static void bindThreadSetName(ThreadSetNameFunc entry);

    static void* threadEntry(void* wall_clock) {
        if (((WallClock*) wall_clock)->_filter_threads) {
            ((WallClock*) wall_clock)->filteredTimerLoop();
        } else {
            ((WallClock*) wall_clock)->timerLoop();
        }
      return NULL;
    }

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);
  public:
    static ThreadSetNameFunc _original_Thread_SetName; 
    static void JNICALL threadSetNameTrap(JNIEnv* env, jobject obj, jstring name);

    const char* name() {
        return _sample_idle_threads ? EVENT_WALL : EVENT_CPU;
    }

    const char* units() {
        return "ns";
    }

    Error start(Arguments& args);
    void stop();
};

#endif // _WALLCLOCK_H
