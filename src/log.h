/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LOG_H
#define _LOG_H

#include <stdarg.h>
#include <stdio.h>

#ifdef __GNUC__
#define ATTR_FORMAT __attribute__((format(printf, 1, 2)))
#else
#define ATTR_FORMAT
#endif


enum LogLevel {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_NONE
};


class Arguments;

class Log {
  private:
    static FILE* _file;
    static LogLevel _level;

  public:
    static const char* const LEVEL_NAME[];

    static void open(Arguments& args);
    static void open(const char* file_name, const char* level);
    static void close();

    static void log(LogLevel level, const char* msg, va_list args);
    static void writeRaw(LogLevel level, const char* msg, size_t len);

    static void ATTR_FORMAT trace(const char* msg, ...);
    static void ATTR_FORMAT debug(const char* msg, ...);
    static void ATTR_FORMAT info(const char* msg, ...);
    static void ATTR_FORMAT warn(const char* msg, ...);
    static void ATTR_FORMAT error(const char* msg, ...);
};

#endif // _LOG_H
