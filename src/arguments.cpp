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
#include <stdlib.h>
#include <string.h>
#include "arguments.h"


// Predefined value that denotes successful operation
const Error Error::OK(NULL);


// Parses agent arguments.
// The format of the string is:
//     arg[,arg...]
// where arg is one of the following options:
//     start         - start profiling
//     stop          - stop profiling
//     status        - print profiling status (inactive / running for X seconds)
//     list          - show the list of available profiling events
//     version       - display the agent version
//     event=EVENT   - which event to trace (cpu, alloc, lock, cache-misses etc.)
//     collapsed[=C] - dump collapsed stacks (the format used by FlameGraph script)
//     svg[=C]       - produce Flame Graph in SVG format
//     tree[=C]      - produce call tree in HTML format
//                     C is counter type: 'samples' or 'total'
//     jfr           - dump events in Java Flight Recorder format
//     summary       - dump profiling summary (number of collected samples of each type)
//     traces[=N]    - dump top N call traces
//     flat[=N]      - dump top N methods (aka flat profile)
//     interval=N    - sampling interval in ns (default: 10'000'000, i.e. 10 ms)
//     jstackdepth=N - maximum Java stack depth (default: MAX_STACK_FRAMES)
//     framebuf=N    - size of the buffer for stack frames (default: 1'000'000)
//     threads       - profile different threads separately
//     allkernel     - include only kernel-mode events
//     alluser       - include only user-mode events
//     simple        - simple class names instead of FQN
//     title=TITLE   - FlameGraph title
//     width=PX      - FlameGraph image width
//     height=PX     - FlameGraph frame height
//     minwidth=PX   - FlameGraph minimum frame width
//     reverse       - generate stack-reversed FlameGraph / Call tree
//     file=FILENAME - output file name for dumping
//
// It is possible to specify multiple dump options at the same time

Error Arguments::parse(const char* args) {
    if (args == NULL) {
        return Error::OK;
    } else if (strlen(args) >= sizeof(_buf)) {
        return Error("Argument list too long");
    }

    strcpy(_buf, args);

    for (char* arg = strtok(_buf, ","); arg != NULL; arg = strtok(NULL, ",")) {
        char* value = strchr(arg, '=');
        if (value != NULL) *value++ = 0;

        if (strcmp(arg, "start") == 0) {
            _action = ACTION_START;
        } else if (strcmp(arg, "stop") == 0) {
            _action = ACTION_STOP;
        } else if (strcmp(arg, "status") == 0) {
            _action = ACTION_STATUS;
        } else if (strcmp(arg, "list") == 0) {
            _action = ACTION_LIST;
        } else if (strcmp(arg, "version") == 0) {
            _action = ACTION_VERSION;
        } else if (strcmp(arg, "event") == 0) {
            if (value == NULL || value[0] == 0) {
                return Error("event must not be empty");
            }
            _event = value;
        } else if (strcmp(arg, "collapsed") == 0 || strcmp(arg, "folded") == 0) {
            _dump_collapsed = true;
            _counter = value == NULL || strcmp(value, "samples") == 0 ? COUNTER_SAMPLES : COUNTER_TOTAL;
        } else if (strcmp(arg, "flamegraph") == 0 || strcmp(arg, "svg") == 0) {
            _dump_flamegraph = true;
            _counter = value == NULL || strcmp(value, "samples") == 0 ? COUNTER_SAMPLES : COUNTER_TOTAL;
        } else if (strcmp(arg, "tree") == 0) {
            _dump_tree = true;
            _counter = value == NULL || strcmp(value, "samples") == 0 ? COUNTER_SAMPLES : COUNTER_TOTAL;
        } else if (strcmp(arg, "jfr") == 0) {
            _dump_jfr = true;
        } else if (strcmp(arg, "summary") == 0) {
            _dump_summary = true;
        } else if (strcmp(arg, "traces") == 0) {
            _dump_traces = value == NULL ? INT_MAX : atoi(value);
        } else if (strcmp(arg, "flat") == 0) {
            _dump_flat = value == NULL ? INT_MAX : atoi(value);
        } else if (strcmp(arg, "interval") == 0) {
            if (value == NULL || (_interval = parseUnits(value)) <= 0) {
                return Error("interval must be > 0");
            }
        } else if (strcmp(arg, "jstackdepth") == 0) {
            if (value == NULL || (_jstackdepth = atoi(value)) <= 0) {
                return Error("jstackdepth must be > 0");
            }
        } else if (strcmp(arg, "framebuf") == 0) {
            if (value == NULL || (_framebuf = atoi(value)) <= 0) {
                return Error("framebuf must be > 0");
            }
        } else if (strcmp(arg, "threads") == 0) {
            _threads = true;
        } else if (strcmp(arg, "allkernel") == 0) {
            _allkernel = true;
        } else if (strcmp(arg, "alluser") == 0) {
            _alluser = true;
        } else if (strcmp(arg, "simple") == 0) {
            _simple = true;
        } else if (strcmp(arg, "title") == 0 && value != NULL) {
            _title = value;
        } else if (strcmp(arg, "width") == 0 && value != NULL) {
            _width = atoi(value);
        } else if (strcmp(arg, "height") == 0 && value != NULL) {
            _height = atoi(value);
        } else if (strcmp(arg, "minwidth") == 0 && value != NULL) {
            _minwidth = atof(value);
        } else if (strcmp(arg, "reverse") == 0) {
            _reverse = true;
        } else if (strcmp(arg, "file") == 0) {
            if (value == NULL || value[0] == 0) {
                return Error("file must not be empty");
            }
            _file = value;
        }
    }

    if (dumpRequested() && (_action == ACTION_NONE || _action == ACTION_STOP)) {
        _action = ACTION_DUMP;
    }

    return Error::OK;
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
