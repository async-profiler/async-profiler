/*
 * Copyright 2023 Andrei Pangin
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

#ifndef _HOOKS_H
#define _HOOKS_H

#include "mutex.h"


class Hooks {
  private:
    static Mutex _patch_lock;
    static int _patched_libs;
    static bool _initialized;

  public:
    static bool init(bool attach);
    static void shutdown();
    static void patchLibraries();

    static bool initialized() {
        return _initialized;
    }
};

#endif // _HOOKS_H
