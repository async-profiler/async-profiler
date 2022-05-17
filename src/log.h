/*
 * Copyright 2021 Andrei Pangin
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

    static void ATTR_FORMAT trace(const char* msg, ...);
    static void ATTR_FORMAT debug(const char* msg, ...);
    static void ATTR_FORMAT info(const char* msg, ...);
    static void ATTR_FORMAT warn(const char* msg, ...);
    static void ATTR_FORMAT error(const char* msg, ...);

    static LogLevel level() { return _level; }
};

#endif // _LOG_H
