/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
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
