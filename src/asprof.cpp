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

#include <fstream>
#include <sstream>
#include "asprof.h"
#include "hooks.h"
#include "profiler.h"


static asprof_error_t asprof_error(const char* msg) {
    return (asprof_error_t)msg;
}


DLLEXPORT void asprof_init() {
    Hooks::init(true);
}

DLLEXPORT const char* asprof_error_str(asprof_error_t err) {
    return err;
}

DLLEXPORT asprof_error_t asprof_execute(const char* command, asprof_writer_t output_callback) {
    Arguments args;
    Error error = args.parse(command);
    if (error) {
        return asprof_error(error.message());
    }

    Log::open(args);

    if (!args.hasOutputFile()) {
        // FIXME: get rid of stream
        std::ostringstream out;
        error = Profiler::instance()->runInternal(args, out);
        if (!error) {
            if (output_callback != NULL) {
                output_callback(out.str().data(), out.str().size());
            }
            return NULL;
        }
    } else {
        std::ofstream out(args.file(), std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            return asprof_error("Could not open output file");
        }
        error = Profiler::instance()->runInternal(args, out);
        out.close();
        if (!error) {
            return NULL;
        }
    }

    return asprof_error(error.message());
}
