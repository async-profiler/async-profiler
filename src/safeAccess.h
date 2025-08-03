/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SAFEACCESS_H
#define _SAFEACCESS_H

#include "arch.h"

class SafeAccess {
  public:
    static void* load(void** ptr, void* default_value = nullptr);
    static u32 load32(u32* ptr, u32 default_value = 0);

    static u32 skipLoad(instruction_t* pc);
};

#endif // _SAFEACCESS_H
