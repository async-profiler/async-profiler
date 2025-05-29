/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CPUENGINE_H
#define _CPUENGINE_H

#include <signal.h>
#include "engine.h"

class CpuEnginePause;

// Base class for CPU sampling engines: PerfEvents, CTimer, ITimer
class CpuEngine : public Engine {
  private:
    static CpuEngine* getCurrent();

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

    virtual void pause() {}
    virtual void resume() {}

  public:
    const char* title() {
        return "CPU profile";
    }

    const char* units() {
        return "ns";
    }

    static void onThreadStart();
    static void onThreadEnd();

    friend CpuEnginePause;
};

class CpuEnginePause {
  private:
    CpuEngine* _current;

  public:
    CpuEnginePause() : _current(CpuEngine::getCurrent()) {
      if (_current != NULL) _current->pause();
    }
    ~CpuEnginePause() {
      if (_current != NULL) _current->resume();
    }
};

#endif // _CPUENGINE_H
