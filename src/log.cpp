/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "log.h"
#include "profiler.h"


const char* const Log::LEVEL_NAME[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "NONE"
};

FILE* Log::_file = stdout;
LogLevel Log::_level = LOG_INFO;


void Log::open(Arguments& args) {
    open(args._log, args._loglevel);

    if (args._unknown_arg != NULL) {
       warn("Unknown argument: %s", args._unknown_arg);
    }
}

void Log::open(const char* file_name, const char* level) {
    if (_file != stdout && _file != stderr) {
        fclose(_file);
    }

    if (file_name == NULL || strcmp(file_name, "stdout") == 0) {
        _file = stdout;
    } else if (strcmp(file_name, "stderr") == 0) {
        _file = stderr;
    } else if ((_file = fopen(file_name, "w")) == NULL) {
        _file = stdout;
        warn("Could not open log file: %s", file_name);
    }

    LogLevel l = LOG_INFO;
    if (level != NULL) {
        for (int i = LOG_TRACE; i <= LOG_NONE; i++) {
            if (strcasecmp(LEVEL_NAME[i], level) == 0) {
                l = (LogLevel)i;
                break;
            }
        }
    }
    __atomic_store_n(&_level, l, __ATOMIC_RELEASE);
}

void Log::close() {
    if (_file != stdout && _file != stderr) {
        fclose(_file);
        _file = stdout;
    }
}

void Log::writeRaw(LogLevel level, const char* msg, size_t len) {
    if (level < _level) {
        return;
    }

    fwrite(msg, 1, len, _file);
    fflush(_file);
}

void Log::log(LogLevel level, const char* msg, va_list args) {
    char buf[1024];
    size_t len = vsnprintf(buf, sizeof(buf), msg, args);
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
        buf[len] = 0;
    }

    if (level < LOG_ERROR) {
        Profiler::instance()->writeLog(level, buf, len);
    }

    if (level >= _level) {
        fprintf(_file, "[%s] %s\n", LEVEL_NAME[level], buf);
        fflush(_file);
    }
}

void Log::trace(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    log(LOG_TRACE, msg, args);
    va_end(args);
}

void Log::debug(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    log(LOG_DEBUG, msg, args);
    va_end(args);
}

void Log::info(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    log(LOG_INFO, msg, args);
    va_end(args);
}

void Log::warn(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    log(LOG_WARN, msg, args);
    va_end(args);
}

void Log::error(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    log(LOG_ERROR, msg, args);
    va_end(args);
}
