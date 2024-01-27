/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

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
        CallbackWriter out(output_callback);
        error = Profiler::instance()->runInternal(args, out);
        if (!error) {
            return NULL;
        }
    } else {
        FileWriter out(args.file());
        if (!out.is_open()) {
            return asprof_error("Could not open output file");
        }
        error = Profiler::instance()->runInternal(args, out);
        if (!error) {
            return NULL;
        }
    }

    return asprof_error(error.message());
}
