/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CPUENGINE_H
#define _CPUENGINE_H

#include <signal.h>
#include "engine.h"


// Base class for CPU sampling engines: PerfEvents, CTimer, ITimer
class CpuEngine : public Engine {
  protected:
    static void** _pthread_entry;
    static CpuEngine* _current;

    static long _interval;
    static CStack _cstack;
    static int _signal;
    static bool _count_overrun;

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);
    static void signalHandlerJ9(int signo, siginfo_t* siginfo, void* ucontext);

    static bool setupThreadHook();

    void enableThreadHook();
    void disableThreadHook();

    bool isResourceLimit(int err);

    int createForAllThreads();

    virtual int createForThread(int tid) { return -1; }
    virtual void destroyForThread(int tid) {}

  public:
    const char* title() {
        return "CPU profile";
    }

    const char* units() {
        return "ns";
    }

    static void onThreadStart();
    static void onThreadEnd();
};

#endif // _CPUENGINE_H
