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
#include <sys/types.h>
#include "arch.h"


typedef void (*SigAction)(int, siginfo_t*, void*);
typedef void (*SigHandler)(int);
typedef void (*TimerCallback)(void*);

// Interrupt threads with this signal. The same signal is used inside JDK to interrupt I/O operations.
const int WAKEUP_SIGNAL = SIGIO;

enum ThreadState {
    THREAD_INVALID,
    THREAD_RUNNING,
    THREAD_UNINTERRUPTIBLE,
    THREAD_SLEEPING
};


class ThreadList {
  public:
    virtual ~ThreadList() {}
    virtual void rewind() = 0;
    virtual int next() = 0;
    virtual int size() = 0;
};


// W^X memory support
class JitWriteProtection {
  private:
    u64 _prev;
    bool _restore;

  public:
    JitWriteProtection(bool enable);
    ~JitWriteProtection();
};


class OS {
  public:
    static const size_t page_size;
    static const size_t page_mask;

    static u64 nanotime();
    static u64 cputime();
    static u64 micros();
    static u64 processStartTime();
    static void sleep(u64 nanos);

    static u64 hton64(u64 x);
    static u64 ntoh64(u64 x);

    static int getMaxThreadId();
    static int processId();
    static int threadId();
    static const char* schedPolicy(int thread_id);
    static bool threadName(int thread_id, char* name_buf, size_t name_len);
    static ThreadState threadState(int thread_id);
    static ThreadList* listThreads();

    static bool isLinux();

    static SigAction installSignalHandler(int signo, SigAction action, SigHandler handler = NULL);
    static SigAction replaceCrashHandler(SigAction action);
    static bool sendSignalToThread(int thread_id, int signo);

    static void* safeAlloc(size_t size);
    static void safeFree(void* addr, size_t size);

    static bool getCpuDescription(char* buf, size_t size);
    static u64 getProcessCpuTime(u64* utime, u64* stime);
    static u64 getTotalCpuTime(u64* utime, u64* stime);

    static void copyFile(int src_fd, int dst_fd, off_t offset, size_t size);
    static void freePageCache(int fd, off_t start_offset);
};

#endif // _OS_H
