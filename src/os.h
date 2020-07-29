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

#ifndef _OS_H
#define _OS_H

#include <signal.h>
#include <stddef.h>
#include "arch.h"


enum ThreadState {
    THREAD_INVALID,
    THREAD_RUNNING,
    THREAD_SLEEPING
};


class ThreadList {
  public:
    virtual ~ThreadList() {}
    virtual void rewind() = 0;
    virtual int next() = 0;
    virtual int size() = 0;
};


class OS {
  private:
    typedef void (*SigAction)(int, siginfo_t*, void*);
    typedef void (*SigHandler)(int);

  public:
    static u64 nanotime();
    static u64 millis();

    static u64 hton64(u64 x);
    static u64 ntoh64(u64 x);

    static int getMaxThreadId();
    static int threadId();
    static bool threadName(int thread_id, char* name_buf, size_t name_len);
    static ThreadState threadState(int thread_id);
    static ThreadList* listThreads();

    static bool isJavaLibraryVisible();

    static void installSignalHandler(int signo, SigAction action, SigHandler handler = NULL);
    static bool sendSignalToThread(int thread_id, int signo);
};

#endif // _OS_H
