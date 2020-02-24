/*
 * Copyright 2017 Andrei Pangin
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "arguments.h"


// Predefined value that denotes successful operation
const Error Error::OK(NULL);

// Extra buffer space for expanding file pattern
const size_t EXTRA_BUF_SIZE = 512;

// Statically compute hash code of a string containing up to 12 [a-z] letters
#define HASH(s)    (HASH12(s "            "))

#define HASH12(s)  (s[0] & 31LL)       | (s[1] & 31LL) <<  5 | (s[2]  & 31LL) << 10 | (s[3]  & 31LL) << 15 | \
                   (s[4] & 31LL) << 20 | (s[5] & 31LL) << 25 | (s[6]  & 31LL) << 30 | (s[7]  & 31LL) << 35 | \
                   (s[8] & 31LL) << 40 | (s[9] & 31LL) << 45 | (s[10] & 31LL) << 50 | (s[11] & 31LL) << 55


// Parses agent arguments.
// The format of the string is:
//     arg[,arg...]
// where arg is one of the following options:
//     start           - start profiling
//     resume          - start or resume profiling without resetting collected data
//     stop            - stop profiling
//     check           - check if the specified profiling event is available
//     status          - print profiling status (inactive / running for X seconds)
//     list            - show the list of available profiling events
//     version[=full]  - display the agent version
//     event=EVENT     - which event to trace (cpu, alloc, lock, cache-misses etc.)
//     collapsed[=C]   - dump collapsed stacks (the format used by FlameGraph script)
//     svg[=C]         - produce Flame Graph in SVG format
//     tree[=C]        - produce call tree in HTML format
//                       C is counter type: 'samples' or 'total'
//     jfr             - dump events in Java Flight Recorder format
//     summary         - dump profiling summary (number of collected samples of each type)
//     traces[=N]      - dump top N call traces
//     flat[=N]        - dump top N methods (aka flat profile)
//     interval=N      - sampling interval in ns (default: 10'000'000, i.e. 10 ms)
//     jstackdepth=N   - maximum Java stack depth (default: 2048)
//     framebuf=N      - size of the buffer for stack frames (default: 1'000'000)
//     file=FILENAME   - output file name for dumping
//     filter=FILTER   - thread filter
//     threads         - profile different threads separately
//     cstack=y|n      - collect C stack frames in addition to Java stack
//     allkernel       - include only kernel-mode events
//     alluser         - include only user-mode events
//     simple          - simple class names instead of FQN
//     dot             - dotted class names
//     sig             - print method signatures
//     ann             - annotate Java method names
//     include=PATTERN - include stack traces containing PATTERN
//     exclude=PATTERN - exclude stack traces containing PATTERN
//     title=TITLE     - FlameGraph title
//     width=PX        - FlameGraph image width
//     height=PX       - FlameGraph frame height
//     minwidth=PX     - FlameGraph minimum frame width
//     reverse         - generate stack-reversed FlameGraph / Call tree
//
// It is possible to specify multiple dump options at the same time

Error Arguments::parse(const char* args) {
    if (args == NULL) {
        return Error::OK;
    }

    size_t len = strlen(args);
    free(_buf);
    _buf = (char*)malloc(len + EXTRA_BUF_SIZE);
    if (_buf == NULL) {
        return Error("Not enough memory to parse arguments");
    }
    strcpy(_buf, args);

    for (char* arg = strtok(_buf, ","); arg != NULL; arg = strtok(NULL, ",")) {
        char* value = strchr(arg, '=');
        if (value != NULL) *value++ = 0;

        switch (hash(arg)) {
            // Actions
            case HASH("start"):
                _action = ACTION_START;
                break;
            case HASH("resume"):
                _action = ACTION_RESUME;
                break;
            case HASH("stop"):
                _action = ACTION_STOP;
                break;
            case HASH("check"):
                _action = ACTION_CHECK;
                break;
            case HASH("status"):
                _action = ACTION_STATUS;
                break;
            case HASH("list"):
                _action = ACTION_LIST;
                break;
            case HASH("version"):
                _action = value == NULL ? ACTION_VERSION : ACTION_FULL_VERSION;
                break;

            // Output formats
            case HASH("collapsed"):
            case HASH("folded"):
                _output = OUTPUT_COLLAPSED;
                _counter = value == NULL || strcmp(value, "samples") == 0 ? COUNTER_SAMPLES : COUNTER_TOTAL;
                break;
            case HASH("flamegraph"):
            case HASH("svg"):
                _output = OUTPUT_FLAMEGRAPH;
                _counter = value == NULL || strcmp(value, "samples") == 0 ? COUNTER_SAMPLES : COUNTER_TOTAL;
                break;
            case HASH("tree"):
                _output = OUTPUT_TREE;
                _counter = value == NULL || strcmp(value, "samples") == 0 ? COUNTER_SAMPLES : COUNTER_TOTAL;
                break;
            case HASH("jfr"):
                _output = OUTPUT_JFR;
                break;
            case HASH("summary"):
                _output = OUTPUT_TEXT;
                break;
            case HASH("traces"):
                _output = OUTPUT_TEXT;
                _dump_traces = value == NULL ? INT_MAX : atoi(value);
                break;
            case HASH("flat"):
                _output = OUTPUT_TEXT;
                _dump_flat = value == NULL ? INT_MAX : atoi(value);
                break;

            // Basic options
            case HASH("event"):
                if (value == NULL || value[0] == 0) {
                    return Error("event must not be empty");
                }
                _event = value;
                break;
            case HASH("interval"):
                if (value == NULL || (_interval = parseUnits(value)) <= 0) {
                    return Error("interval must be > 0");
                }
                break;
            case HASH("jstackdepth"):
                if (value == NULL || (_jstackdepth = atoi(value)) <= 0) {
                    return Error("jstackdepth must be > 0");
                }
                break;
            case HASH("framebuf"):
                if (value == NULL || (_framebuf = atoi(value)) <= 0) {
                    return Error("framebuf must be > 0");
                }
                break;
            case HASH("file"):
                if (value == NULL || value[0] == 0) {
                    return Error("file must not be empty");
                }
                _file = value;
                break;

            // Filters
            case HASH("filter"):
                _filter = value == NULL ? "" : value;
                break;
            case HASH("include"):
                if (value != NULL) appendToEmbeddedList(_include, value);
                break;
            case HASH("exclude"):
                if (value != NULL) appendToEmbeddedList(_exclude, value);
                break;
            case HASH("threads"):
                _threads = true;
                break;
            case HASH("cstack"):
                _cstack = value == NULL ? 'y' : value[0];
                break;
            case HASH("allkernel"):
                _ring = RING_KERNEL;
                break;
            case HASH("alluser"):
                _ring = RING_USER;
                break;

            // Output style modifiers
            case HASH("simple"):
                _style |= STYLE_SIMPLE;
                break;
            case HASH("dot"):
                _style |= STYLE_DOTTED;
                break;
            case HASH("sig"):
                _style |= STYLE_SIGNATURES;
                break;
            case HASH("ann"):
                _style |= STYLE_ANNOTATE;
                break;

            // FlameGraph options
            case HASH("title"):
                if (value != NULL) _title = value;
                break;
            case HASH("width"):
                if (value != NULL) _width = atoi(value);
                break;
            case HASH("height"):
                if (value != NULL) _height = atoi(value);
                break;
            case HASH("minwidth"):
                if (value != NULL) _minwidth = atof(value);
                break;
            case HASH("reverse"):
                _reverse = true;
                break;
        }
    }

    if (_file != NULL && strchr(_file, '%') != NULL) {
        _file = expandFilePattern(_buf + len + 1, EXTRA_BUF_SIZE - 1, _file);
    }

    if (_file != NULL && _output == OUTPUT_NONE) {
        _output = detectOutputFormat(_file);
        _dump_traces = 200;
        _dump_flat = 200;
    }

    if (_output != OUTPUT_NONE && (_action == ACTION_NONE || _action == ACTION_STOP)) {
        _action = ACTION_DUMP;
    }

    return Error::OK;
}

// The linked list of string offsets is embedded right into _buf array
void Arguments::appendToEmbeddedList(int& list, char* value) {
    ((int*)value)[-1] = list;
    list = (int)(value - _buf);
}

// Should match statically computed HASH(arg)
long long Arguments::hash(const char* arg) {
    long long h = 0;
    for (int shift = 0; *arg != 0; shift += 5) {
        h |= (*arg++ & 31LL) << shift;
    }
    return h;
}

// Expands %p to the process id
//         %t to the timestamp
const char* Arguments::expandFilePattern(char* dest, size_t max_size, const char* pattern) {
    char* ptr = dest;
    char* end = dest + max_size - 1;

    while (ptr < end && *pattern != 0) {
        char c = *pattern++;
        if (c == '%') {
            c = *pattern++;
            if (c == 0) {
                break;
            } else if (c == 'p') {
                ptr += snprintf(ptr, end - ptr, "%d", getpid());
                continue;
            } else if (c == 't') {
                time_t timestamp = time(NULL);
                struct tm t;
                localtime_r(&timestamp, &t);
                ptr += snprintf(ptr, end - ptr, "%d%02d%02d-%02d%02d%02d",
                                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                                t.tm_hour, t.tm_min, t.tm_sec);
                continue;
            }
        }
        *ptr++ = c;
    }

    *ptr = 0;
    return dest;
}

Output Arguments::detectOutputFormat(const char* file) {
    const char* ext = strrchr(file, '.');
    if (ext != NULL) {
        if (strcmp(ext, ".svg") == 0) {
            return OUTPUT_FLAMEGRAPH;
        } else if (strcmp(ext, ".html") == 0) {
            return OUTPUT_TREE;
        } else if (strcmp(ext, ".jfr") == 0) {
            return OUTPUT_JFR;
        } else if (strcmp(ext, ".collapsed") == 0 || strcmp(ext, ".folded") == 0) {
            return OUTPUT_COLLAPSED;
        }
    }
    return OUTPUT_TEXT;
}

long Arguments::parseUnits(const char* str) {
    char* end;
    long result = strtol(str, &end, 0);

    if (*end) {
        switch (*end) {
            case 'K': case 'k':
            case 'U': case 'u': // microseconds
                return result * 1000;
            case 'M': case 'm': // million, megabytes or milliseconds
                return result * 1000000;
            case 'G': case 'g':
            case 'S': case 's': // seconds
                return result * 1000000000;
        }
    }

    return result;
}

Arguments::~Arguments() {
    free(_buf);
}

void Arguments::save(Arguments& other) {
    free(_buf);
    *this = other;
    other._buf = NULL;
}
