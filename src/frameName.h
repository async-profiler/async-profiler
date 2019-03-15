/*
 * Copyright 2017 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _FRAMENAME_H
#define _FRAMENAME_H

#include <jvmti.h>
#include <locale.h>
#include <map>
#include <string>
#include "mutex.h"
#include "vmEntry.h"

#ifdef __APPLE__
#  include <xlocale.h>
#endif


typedef std::map<jmethodID, std::string> JMethodCache;
typedef std::map<int, std::string> ThreadMap;

class FrameName {
  private:
    JMethodCache _cache;
    char _buf[800];  // must be large enough for class name + method name + method signature
    int _style;
    Mutex& _thread_names_lock;
    ThreadMap& _thread_names;
    locale_t _saved_locale;

    char* truncate(char* name, int max_length);
    const char* cppDemangle(const char* name);
    char* javaMethodName(jmethodID method, char type);
    char* javaClassName(const char* symbol, int length, int style);

  public:
    FrameName(int style, Mutex& thread_names_lock, ThreadMap& thread_names);
    ~FrameName();

    const char* name(ASGCT_CallFrame& frame);
};

#endif // _FRAMENAME_H
