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
#include "arch.h"


class ThreadList {
  public:
    virtual ~ThreadList() {}
    virtual int next() = 0;
};


class OS {
  public:
    static u64 nanotime();
    static u64 millis();
    static u64 hton64(u64 x);
    static int threadId();
    static bool isThreadRunning(int thread_id);
    static bool isSignalSafeTLS();
    static bool isJavaLibraryVisible();
    static void installSignalHandler(int signo, void (*handler)(int, siginfo_t*, void*));
    static void sendSignalToThread(int thread_id, int signo);
    static ThreadList* listThreads();
};

#endif // _OS_H
