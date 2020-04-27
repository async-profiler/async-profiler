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

#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include <set>
#include "codeCache.h"
#include "mutex.h"


class Symbols {
  private:
    static Mutex _parse_lock;
    static std::set<const void*> _parsed_libraries;
    static bool _have_kernel_symbols;

  public:
    static void parseKernelSymbols(NativeCodeCache* cc);
    static void parseLibraries(NativeCodeCache** array, volatile int& count, int size);

    static bool haveKernelSymbols() {
        return _have_kernel_symbols;
    }
};

#endif // _SYMBOLS_H
