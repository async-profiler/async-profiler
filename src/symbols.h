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

#include "codeCache.h"


class BuildId {
  private:
    unsigned char _value[64];
    int _length;

  public:
    BuildId() : _length(0) {
    }

    int length() {
        return _length;
    }

    unsigned char operator[](int index) {
        return _value[index];
    }

    void set(const char* value, int length);
};

class Symbols {
  private:
    static void parseKernelSymbols(CodeCache* cc);
    static void parseLibrarySymbols(CodeCache* cc, const char* lib_name, const char* base);
    static bool parseFile(CodeCache* cc, const char* file_name, const char* base, BuildId* build_id);
    static void parseElf(CodeCache* cc, const char* addr, const char* base, BuildId* build_id);

  public:
    static int parseMaps(CodeCache** array, int size);
};

#endif // _SYMBOLS_H
