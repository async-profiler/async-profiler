/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <stdlib.h>
#include "hooks.h"
#include "profiler.h"


// This should be called only after all other statics are initialized.
// Therefore, put it in the last file in the alphabetic order.
class LateInitializer {
  public:
    LateInitializer() {
        const char* command = getenv("ASPROF_COMMAND");
        if (command != NULL && !isJavaApp() && Hooks::init(false)) {
            startProfiler(command);
        }
    }

  private:
    bool isJavaApp() {
        void* libjvm = dlopen(OS::isLinux() ? "libjvm.so" : "libjvm.dylib", RTLD_LAZY | RTLD_NOLOAD);
        if (libjvm != NULL) {
            dlclose(libjvm);
            return true;
        }
        return false;
    }

    void startProfiler(const char* command) {
        Error error = _global_args.parse(command);
        _global_args._preloaded = true;

        Log::open(_global_args);

        if (error || (error = Profiler::instance()->run(_global_args))) {
            Log::error("%s", error.message());
        }
    }
};

static LateInitializer _late_initializer;
