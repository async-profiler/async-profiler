/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
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

Mutex Log::_lock;
int Log::_fd = STDOUT_FILENO;
LogLevel Log::_level = LOG_INFO;


void Log::open(Arguments& args) {
    open(args._log, args._loglevel);

    if (args._unknown_arg != NULL) {
       warn("Unknown argument: %s", args._unknown_arg);
    }
}

void Log::open(const char* file_name, const char* level) {
    LogLevel l = LOG_INFO;
    if (level != NULL) {
        for (int i = LOG_TRACE; i <= LOG_NONE; i++) {
            if (strcasecmp(LEVEL_NAME[i], level) == 0) {
                l = (LogLevel)i;
                break;
            }
        }
    }

    MutexLocker ml(_lock);
    _level = l;

    if (_fd > STDERR_FILENO) {
        ::close(_fd);
    }

    if (file_name == NULL || strcmp(file_name, "stdout") == 0) {
        _fd = STDOUT_FILENO;
    } else if (strcmp(file_name, "stderr") == 0) {
        _fd = STDERR_FILENO;
    } else if ((_fd = creat(file_name, 0660)) < 0) {
        _fd = STDOUT_FILENO;
        warn("Could not open log file: %s", file_name);
    }
}

void Log::close() {
    MutexLocker ml(_lock);
    if (_fd > STDERR_FILENO) {
        ::close(_fd);
        _fd = STDOUT_FILENO;
    }
}

void Log::writeRaw(LogLevel level, const char* msg, size_t len) {
    MutexLocker ml(_lock);
    if (level < _level) {
        return;
    }

    while (len > 0) {
        ssize_t bytes = ::write(_fd, msg, len);
        if (bytes <= 0) {
            break;
        }
        msg += (size_t)bytes;
        len -= (size_t)bytes;
    }
}

void Log::log(LogLevel level, const char* msg, va_list args) {
    char buf[1024];

    // Format log message: [LEVEL] Message\n
    size_t prefix_len = snprintf(buf, sizeof(buf), "[%s] ", LEVEL_NAME[level]);
    size_t msg_len = vsnprintf(buf + prefix_len, sizeof(buf) - 1 - prefix_len, msg, args);
    if (msg_len > sizeof(buf) - 1 - prefix_len) {
        msg_len = sizeof(buf) - 1 - prefix_len;
    }
    buf[prefix_len + msg_len] = '\n';

    // Write all messages to JFR, if active
    if (level < LOG_ERROR) {
        Profiler::instance()->writeLog(level, buf + prefix_len, msg_len);
    }

    // Write a message with a prefix to a file
    if (level >= _level) {
        writeRaw(level, buf, prefix_len + msg_len + 1);
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
