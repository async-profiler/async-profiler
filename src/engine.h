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

#ifndef _ENGINE_H
#define _ENGINE_H

#include "arguments.h"
#include "codeCache.h"

class Engine {
  public:
    virtual const char* name() = 0;
    virtual const char* units() = 0;

    virtual Error start(Arguments& args) = 0;
    virtual void stop() = 0;

    virtual void onThreadStart() {}
    virtual void onThreadEnd() {}

    virtual int getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth,
                               VmCodeCache* cc);
};

#endif // _ENGINE_H
