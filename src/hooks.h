/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _HOOKS_H
#define _HOOKS_H

#include "mutex.h"


class Hooks {
  private:
    static Mutex _patch_lock;
    static int _patched_libs;
    static bool _initialized;

  public:
    static bool init(bool attach);
    static void shutdown();
    static void patchLibraries();

    static bool initialized() {
        return _initialized;
    }
};

#endif // _HOOKS_H
