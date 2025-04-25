/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _USEREVENTS_H
#define _USEREVENTS_H

#include <map>
#include "dictionary.h"

class UserEvents {
  private:
    static Dictionary _dict;

  public:
    static int registerEvent(const char* event);
    static void collect(std::map<unsigned int, const char*>& map);
};

#endif // _USEREVENTS_H
