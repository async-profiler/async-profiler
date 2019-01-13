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

#ifndef _TRACER_H
#define _TRACER_H

#include <pthread.h>
#include <stdio.h>
#include "arch.h"
#include "arguments.h"


class Tracer {
  private:
    FILE* _out;
    int _pipefd[2];
    pthread_t _thread;
    volatile int _tracer_tid;
    bool _simple;
    bool _annotate;
    bool _running;

    void tracerLoop();

    static void* threadEntry(void* tracer) {
        ((Tracer*)tracer)->tracerLoop();
        return NULL;
    }

  public:
    Tracer() : _running(false) {
    }

    Error start(Arguments& args);
    void stop();

    void recordExecutionSample(int tid, int call_trace_id, u64 counter);
};

#endif // _TRACER_H
