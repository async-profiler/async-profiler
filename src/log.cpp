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

#include <string.h>
#include "log.h"


FILE* Log::_file = stdout;


void Log::open(const char* file_name) {
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
}

void Log::close() {
    if (_file != stdout && _file != stderr) {
        fclose(_file);
        _file = stdout;
    }
}

inline void Log::log(const char* level, const char* msg, va_list args) {
    fputs(level, _file);
    vfprintf(_file, msg, args);
    fputs("\n", _file);
}

void Log::info(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    log("[INFO] ", msg, args);
    va_end(args);
}

void Log::warn(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    log("[WARN] ", msg, args);
    va_end(args);
}

void Log::error(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    log("[ERROR] ", msg, args);
    va_end(args);
}
