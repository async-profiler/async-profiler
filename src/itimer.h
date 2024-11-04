/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ITIMER_H
#define _ITIMER_H

#include "cpuEngine.h"


class ITimer : public CpuEngine {
  public:
    const char* type() {
        return "itimer";
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();
};

#endif // _ITIMER_H
