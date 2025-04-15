/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asprof.h"
#include "hooks.h"
#include "profiler.h"
#include "tsc.h"
#include "threadLocalData.h"
#include "userEvents.h"

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

DLLEXPORT asprof_thread_local_data* asprof_get_thread_local_data(void) {
    return ThreadLocalData::getThreadLocalData();
}

DLLEXPORT asprof_jfr_event_key asprof_register_jfr_event(const char* name) {
    return UserEvents::registerEvent(name);
}

#define asprof_str(s) #s

DLLEXPORT asprof_error_t asprof_emit_jfr_event(asprof_jfr_event_key type, const uint8_t* data, size_t len) {
    if (len > ASPROF_MAX_JFR_EVENT_LENGTH) {
        return asprof_error("Unable to emit JFR event larger than " asprof_str(ASPROF_MAX_JFR_EVENT_LENGTH) " bytes");
    }

    UserEvent event;
    event._start_time = TSC::ticks();
    event._type = type;
    event._data = data;
    event._len = len;
    Profiler::instance()->recordEventOnly(USER_EVENT, &event);
    return NULL;
}
