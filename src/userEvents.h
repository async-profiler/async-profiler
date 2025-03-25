/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _USEREVENT_H
#define _USEREVENT_H
 
#include <map>
#include <stddef.h>
#include "mutex.h"
#include "dictionary.h"

class UserEvents {
  public:
    static int registerEvent(const char *event);
    static void collect(std::map<unsigned int, const char*>& map);
  private:
    static Mutex _state_lock;
    static Dictionary _dict;
};

#endif
