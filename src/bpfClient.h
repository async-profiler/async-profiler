/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BPFCLIENT_H
#define _BPFCLIENT_H

#include <signal.h>
#include "engine.h"


class BpfClient : public Engine {
  private:

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);

  public:
    const char* title() {
        return "CPU profile";
    }

    const char* units() {
        return "cycles";
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    static int walk(int tid, void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx);

    static const char* schedPolicy(int tid);
};

#endif // _BPFCLIENT_H
