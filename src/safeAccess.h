/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SAFEACCESS_H
#define _SAFEACCESS_H

#include <stdint.h>

class StackFrame;

class SafeAccess {
  public:
    static void* load(void** ptr, void* default_value = nullptr);
    static int32_t load32(int32_t* ptr, int32_t default_value = 0);

    static bool checkFault(StackFrame& frame);
};

#endif // _SAFEACCESS_H
