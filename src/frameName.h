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

#include "vmEntry.h"


class FrameName {
  private:
    char _buf[520];
    const char* _str;

    const char* cppDemangle(const char* name);
    const char* javaClassName(const char* symbol, int length);

  public:
    FrameName(ASGCT_CallFrame& frame, bool dotted = false);

    const char* toString() {
        return _str;
    }
};

#endif // _FRAMENAME_H
