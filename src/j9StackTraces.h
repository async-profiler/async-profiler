/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
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
