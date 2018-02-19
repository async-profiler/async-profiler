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
#include <map>
#include <string>
#include "vmEntry.h"
#include "vmStructs.h"


class ThreadId {
  private:
    int _id;
    const char* _name;

  public:
    static int comparator(const void* t1, const void* t2) {
        return ((ThreadId*)t1)->_id - ((ThreadId*)t2)->_id;
    }

    friend class FrameName;
};


typedef std::map<jmethodID, std::string> JMethodCache;

class FrameName {
  private:
    JMethodCache _cache;
    char _buf[520];
    bool _dotted;
    int _thread_count;
    ThreadId* _threads;

    const char* findThreadName(int tid);
    const char* cppDemangle(const char* name);
    const char* javaMethodName(jmethodID method, bool dotted);
    char* javaClassName(VMKlass* klass);
    char* javaClassName(const char* symbol, int length, bool dotted);

  public:
    FrameName(bool dotted = false);
    ~FrameName();

    const char* name(ASGCT_CallFrame& frame);
};

#endif // _FRAMENAME_H
