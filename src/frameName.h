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
    char _buf[520];
    bool _simple;
    bool _annotate;
    bool _dotted;
    Mutex& _thread_names_lock;
    ThreadMap& _thread_names;
    locale_t _saved_locale;

    const char* cppDemangle(const char* name);
    char* javaMethodName(jmethodID method);
    char* javaClassName(const char* symbol, int length, bool simple, bool dotted);

  public:
    FrameName(bool simple, bool annotate, bool dotted, Mutex& thread_names_lock, ThreadMap& thread_names);
    ~FrameName();

    const char* name(ASGCT_CallFrame& frame);
};

#endif // _FRAMENAME_H
