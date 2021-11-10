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


class Engine {
  protected:
    static volatile bool _enabled;

  public:
    virtual const char* title() {
        return "Flame Graph";
    }

    virtual const char* units() {
        return "total";
    }

    virtual Error check(Arguments& args);
    virtual Error start(Arguments& args);
    virtual void stop();

    virtual int getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth);

    void enableEvents(bool enabled) {
        _enabled = enabled;
    }
};

#endif // _ENGINE_H
