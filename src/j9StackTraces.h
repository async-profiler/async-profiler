/*
 * Copyright 2021 Andrei Pangin
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

#ifndef _J9STACKTRACES_H
#define _J9STACKTRACES_H

#include <pthread.h>
#include "arch.h"
#include "arguments.h"


const int MAX_J9_NATIVE_FRAMES = 128;

struct J9StackTraceNotification {
    void* env;
    u64 counter;
    int num_frames;
    int reserved;
    const void* addr[MAX_J9_NATIVE_FRAMES];

    size_t size() {
        return sizeof(*this) - sizeof(this->addr) + num_frames * sizeof(const void*);
    }
};


class J9StackTraces {
  private:
    static pthread_t _thread;
    static int _max_stack_depth;
    static int _pipe[2];

    static void* threadEntry(void* unused) {
        timerLoop();
        return NULL;
    }

    static void timerLoop();

  public:
    static Error start(Arguments& args);
    static void stop();

    static void checkpoint(u64 counter, J9StackTraceNotification* notif);
};

#endif // _J9STACKTRACES_H
