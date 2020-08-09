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

// Simulate switch statement over string hashes
#define SWITCH(arg)    long long arg_hash = hash(arg); if (0)

#define CASE(s)        } else if (arg_hash == HASH(s)) {

#define CASE2(s1, s2)  } else if (arg_hash == HASH(s1) || arg_hash == HASH(s2)) {


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
//     safemode=BITS   - disable stack recovery techniques (default: 0, i.e. everything enabled)
//     file=FILENAME   - output file name for dumping
//     filter=FILTER   - thread filter
//     threads         - profile different threads separately
//     cstack=MODE     - how to collect C stack frames in addition to Java stack
//                       MODE is 'fp' (Frame Pointer), 'lbr' (Last Branch Record) or 'no'
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

        SWITCH (arg) {
            // Actions
            CASE("start")
                _action = ACTION_START;

            CASE("resume")
                _action = ACTION_RESUME;

            CASE("stop")
                _action = ACTION_STOP;

            CASE("check")
                _action = ACTION_CHECK;

            CASE("status")
                _action = ACTION_STATUS;

            CASE("list")
                _action = ACTION_LIST;

            CASE("version")
                _action = value == NULL ? ACTION_VERSION : ACTION_FULL_VERSION;

            // Output formats
            CASE2("collapsed", "folded")
                _output = OUTPUT_COLLAPSED;
                _counter = value == NULL || strcmp(value, "samples") == 0 ? COUNTER_SAMPLES : COUNTER_TOTAL;

            CASE2("flamegraph", "svg")
                _output = OUTPUT_FLAMEGRAPH;
                _counter = value == NULL || strcmp(value, "samples") == 0 ? COUNTER_SAMPLES : COUNTER_TOTAL;

            CASE("tree")
                _output = OUTPUT_TREE;
                _counter = value == NULL || strcmp(value, "samples") == 0 ? COUNTER_SAMPLES : COUNTER_TOTAL;

            CASE("jfr")
                _output = OUTPUT_JFR;

            CASE("summary")
                _output = OUTPUT_TEXT;

            CASE("traces")
                _output = OUTPUT_TEXT;
                _dump_traces = value == NULL ? INT_MAX : atoi(value);

            CASE("flat")
                _output = OUTPUT_TEXT;
                _dump_flat = value == NULL ? INT_MAX : atoi(value);

            // Basic options
            CASE("event")
                if (value == NULL || value[0] == 0) {
                    return Error("event must not be empty");
                }
                _event = value;

            CASE("interval")
                if (value == NULL || (_interval = parseUnits(value)) <= 0) {
                    return Error("Invalid interval");
                }

            CASE("jstackdepth")
                if (value == NULL || (_jstackdepth = atoi(value)) <= 0) {
                    return Error("jstackdepth must be > 0");
                }

            CASE("framebuf")
                if (value == NULL || (_framebuf = atoi(value)) <= 0) {
                    return Error("framebuf must be > 0");
                }

            CASE("safemode")
                _safe_mode = value == NULL ? INT_MAX : atoi(value);

            CASE("file")
                if (value == NULL || value[0] == 0) {
                    return Error("file must not be empty");
                }
                _file = value;

            // Filters
            CASE("filter")
                _filter = value == NULL ? "" : value;

            CASE("include")
                if (value != NULL) appendToEmbeddedList(_include, value);

            CASE("exclude")
                if (value != NULL) appendToEmbeddedList(_exclude, value);

            CASE("threads")
                _threads = true;

            CASE("allkernel")
                _ring = RING_KERNEL;

            CASE("alluser")
                _ring = RING_USER;

            CASE("cstack")
                if (value != NULL) {
                    if (value[0] == 'n') {
                        _cstack = CSTACK_NO;
                    } else if (value[0] == 'l') {
                        _cstack = CSTACK_LBR;
                    } else {
                        _cstack = CSTACK_FP;
                    }
                }

            // Output style modifiers
            CASE("simple")
                _style |= STYLE_SIMPLE;

            CASE("dot")
                _style |= STYLE_DOTTED;

            CASE("sig")
                _style |= STYLE_SIGNATURES;

            CASE("ann")
                _style |= STYLE_ANNOTATE;

            // FlameGraph options
            CASE("title")
                if (value != NULL) _title = value;

            CASE("width")
                if (value != NULL) _width = atoi(value);

            CASE("height")
                if (value != NULL) _height = atoi(value);

            CASE("minwidth")
                if (value != NULL) _minwidth = atof(value);

            CASE("reverse")
                _reverse = true;
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

    switch (*end) {
        case 0:
            return result;
        case 'K': case 'k':
        case 'U': case 'u': // microseconds
            return result * 1000;
        case 'M': case 'm': // million, megabytes or milliseconds
            return result * 1000000;
        case 'G': case 'g':
        case 'S': case 's': // seconds
            return result * 1000000000;
    }

    return -1;
}

Arguments::~Arguments() {
    free(_buf);
}

void Arguments::save(Arguments& other) {
    free(_buf);
    *this = other;
    other._buf = NULL;
}
