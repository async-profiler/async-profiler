/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <signal.h>
#include <dlfcn.h>
#include "asprof.h"

#ifdef __linux__

int (*sigaction_func)(int, const struct sigaction *, struct sigaction *) = NULL;

extern "C" WEAK DLLEXPORT
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (sigaction_func == NULL) {
        sigaction_func = (decltype(sigaction_func))dlsym(RTLD_NEXT, "sigaction");
    }
    return sigaction_func(signum, act, oldact);
}

#else

extern "C" DLLEXPORT
int mac_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    return sigaction(signum, act, oldact);
}

__attribute__((used)) static struct {
    const void *replacement;
    const void *original;
} interposers[] __attribute__((section("__DATA,__interpose"))) = {
    { (const void *)mac_sigaction, (const void *)sigaction }
};

#endif
