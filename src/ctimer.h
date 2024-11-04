/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CTIMER_H
#define _CTIMER_H

#include "cpuEngine.h"

#ifdef __linux__

class CTimer : public CpuEngine {
  private:
    static int _max_timers;
    static int* _timers;

    int createForThread(int tid);
    void destroyForThread(int tid);

  public:
    const char* type() {
        return "ctimer";
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    static bool supported() {
        return true;
    }
};

#else

class CTimer : public CpuEngine {
  public:
    Error check(Arguments& args) {
        return Error("CTimer is not supported on this platform");
    }

    Error start(Arguments& args) {
        return Error("CTimer is not supported on this platform");
    }

    static bool supported() {
        return false;
    }
};

#endif // __linux__

#endif // _CTIMER_H
