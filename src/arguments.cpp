/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "arguments.h"


// Arguments of the last start/resume command; reused for shutdown and restart
Arguments _global_args;

// Predefined value that denotes successful operation
const Error Error::OK(NULL);

// Extra buffer space for expanding file pattern
const size_t EXTRA_BUF_SIZE = 512;

static const Multiplier NANOS[] = {{'n', 1}, {'u', 1000}, {'m', 1000000}, {'s', 1000000000}, {0, 0}};
static const Multiplier BYTES[] = {{'b', 1}, {'k', 1024}, {'m', 1048576}, {'g', 1073741824}, {0, 0}};
static const Multiplier SECONDS[] = {{'s', 1}, {'m', 60}, {'h', 3600}, {'d', 86400}, {0, 0}};
static const Multiplier UNIVERSAL[] = {{'n', 1}, {'u', 1000}, {'m', 1000000}, {'s', 1000000000}, {'b', 1}, {'k', 1024}, {'g', 1073741824}, {0, 0}};


// Statically compute hash code of a string containing up to 12 [a-z] letters
#define HASH(s)  ((s[0] & 31LL)       | (s[1] & 31LL) <<  5 | (s[2]  & 31LL) << 10 | (s[3]  & 31LL) << 15 | \
                  (s[4] & 31LL) << 20 | (s[5] & 31LL) << 25 | (s[6]  & 31LL) << 30 | (s[7]  & 31LL) << 35 | \
                  (s[8] & 31LL) << 40 | (s[9] & 31LL) << 45 | (s[10] & 31LL) << 50 | (s[11] & 31LL) << 55)

// Simulate switch statement over string hashes
#define SWITCH(arg)    long long arg_hash = hash(arg); if (0)

#define CASE(s)        } else if (arg_hash == HASH(s "            ")) {

#define DEFAULT()      } else {


// Parses agent arguments.
// The format of the string is:
//     arg[,arg...]
// where arg is one of the following options:
//     start            - start profiling
//     resume           - start or resume profiling without resetting collected data
//     stop             - stop profiling
//     dump             - dump collected data without stopping profiling session
//     check            - check if the specified profiling event is available
//     status           - print profiling status (inactive / running for X seconds)
//     meminfo          - print profiler memory stats
//     list             - show the list of available profiling events
//     version          - display the agent version
//     event=EVENT      - which event to trace (cpu, wall, cache-misses, etc.)
//     alloc[=BYTES]    - profile allocations with BYTES interval
//     live             - build allocation profile from live objects only
//     lock[=DURATION]  - profile contended locks overflowing the DURATION ns bucket (default: 10us)
//     wall[=NS]        - run wall clock profiling together with CPU profiling
//     collapsed        - dump collapsed stacks (the format used by FlameGraph script)
//     flamegraph       - produce Flame Graph in HTML format
//     tree             - produce call tree in HTML format
//     jfr              - dump events in Java Flight Recorder format
//     jfropts=OPTIONS  - JFR recording options: numeric bitmask or 'mem'
//     jfrsync[=CONFIG] - start Java Flight Recording with the given config along with the profiler
//     traces[=N]       - dump top N call traces
//     flat[=N]         - dump top N methods (aka flat profile)
//     samples          - count the number of samples (default)
//     total            - count the total value (time, bytes, etc.) instead of samples
//     chunksize=N      - approximate size of JFR chunk in bytes (default: 100 MB)
//     chunktime=N      - duration of JFR chunk in seconds (default: 1 hour)
//     timeout=TIME     - automatically stop profiler at TIME (absolute or relative)
//     loop=TIME        - run profiler in a loop (continuous profiling)
//     interval=N       - sampling interval in ns (default: 10'000'000, i.e. 10 ms)
//     jstackdepth=N    - maximum Java stack depth (default: 2048)
//     signal=N         - use alternative signal for cpu or wall clock profiling
//     features=LIST    - advanced stack trace features (vtable, comptask, pcaddr)"
//     safemode=BITS    - disable stack recovery techniques (default: 0, i.e. everything enabled)
//     file=FILENAME    - output file name for dumping
//     log=FILENAME     - log warnings and errors to the given dedicated stream
//     loglevel=LEVEL   - logging level: TRACE, DEBUG, INFO, WARN, ERROR, or NONE
//     server=ADDRESS   - start insecure HTTP server at ADDRESS/PORT
//     filter=FILTER    - thread filter
//     threads          - profile different threads separately
//     sched            - group threads by scheduling policy
//     cstack=MODE      - how to collect C stack frames in addition to Java stack
//                        MODE is 'fp', 'dwarf', 'lbr', 'vm' or 'no'
//     clock=SOURCE     - clock source for JFR timestamps: 'tsc' or 'monotonic'
//     allkernel        - include only kernel-mode events
//     alluser          - include only user-mode events
//     fdtransfer       - use fdtransfer to pass fds to the profiler
//     simple           - simple class names instead of FQN
//     dot              - dotted class names
//     norm             - normalize names of hidden classes / lambdas
//     sig              - print method signatures
//     ann              - annotate Java methods
//     lib              - prepend library names
//     mcache           - max age of jmethodID cache (default: 0 = disabled)
//     include=PATTERN  - include stack traces containing PATTERN
//     exclude=PATTERN  - exclude stack traces containing PATTERN
//     begin=FUNCTION   - begin profiling when FUNCTION is executed
//     end=FUNCTION     - end profiling when FUNCTION is executed
//     title=TITLE      - FlameGraph title
//     minwidth=PCT     - FlameGraph minimum frame width in percent
//     reverse          - generate stack-reversed FlameGraph / Call tree
//
// It is possible to specify multiple dump options at the same time

Error Arguments::parse(const char* args) {
    if (args == NULL) {
        return Error::OK;
    }

    size_t len = strlen(args);
    free(_buf);
    _buf = (char*)malloc(len + EXTRA_BUF_SIZE + 1);
    if (_buf == NULL) {
        return Error("Not enough memory to parse arguments");
    }
    char* args_copy = strcpy(_buf + EXTRA_BUF_SIZE, args);

    const char* msg = NULL;    

    for (char* arg = strtok(args_copy, ","); arg != NULL; arg = strtok(NULL, ",")) {
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

            CASE("dump")
                _action = ACTION_DUMP;

            CASE("check")
                _action = ACTION_CHECK;

            CASE("status")
                _action = ACTION_STATUS;

            CASE("meminfo")
                _action = ACTION_MEMINFO;

            CASE("list")
                _action = ACTION_LIST;

            CASE("version")
                _action = ACTION_VERSION;

            // Output formats
            CASE("collapsed")
                _output = OUTPUT_COLLAPSED;

            CASE("flamegraph")
                _output = OUTPUT_FLAMEGRAPH;

            CASE("tree")
                _output = OUTPUT_TREE;

            CASE("jfr")
                _output = OUTPUT_JFR;

            CASE("jfropts")
                _output = OUTPUT_JFR;
                if (value == NULL) {
                    msg = "Invalid jfropts";
                } else if (value[0] >= '0' && value[0] <= '9') {
                    _jfr_options = (int)strtol(value, NULL, 0);
                } else if (strstr(value, "mem")) {
                    _jfr_options |= IN_MEMORY;
                }

            CASE("jfrsync")
                _output = OUTPUT_JFR;
                _jfr_options |= JFR_SYNC_OPTS;
                _jfr_sync = value == NULL ? "default" : value;

            CASE("traces")
                _output = OUTPUT_TEXT;
                _dump_traces = value == NULL ? INT_MAX : atoi(value);

            CASE("flat")
                _output = OUTPUT_TEXT;
                _dump_flat = value == NULL ? INT_MAX : atoi(value);

            CASE("samples")
                _counter = COUNTER_SAMPLES;

            CASE("total")
                _counter = COUNTER_TOTAL;

            CASE("chunksize")
                if (value == NULL || (_chunk_size = parseUnits(value, BYTES)) < 0) {
                    msg = "Invalid chunksize";
                }

            CASE("chunktime")
                if (value == NULL || (_chunk_time = parseUnits(value, SECONDS)) < 0) {
                    msg = "Invalid chunktime";
                }

            // Basic options
            CASE("event")
                if (value == NULL || value[0] == 0) {
                    msg = "event must not be empty";
                } else if (strcmp(value, EVENT_ALLOC) == 0) {
                    if (_alloc < 0) _alloc = 0;
                } else if (strcmp(value, EVENT_LOCK) == 0) {
                    if (_lock < 0) _lock = DEFAULT_LOCK_INTERVAL;
                } else if (_event != NULL) {
                    msg = "Duplicate event argument";
                } else {
                    _event = value;
                }

            CASE("timeout")
                if (value == NULL || (_timeout = parseTimeout(value)) == -1) {
                    msg = "Invalid timeout";
                }

            CASE("loop")
                _loop = true;
                if (value == NULL || (_timeout = parseTimeout(value)) == -1) {
                    msg = "Invalid loop duration";
                }

            CASE("alloc")
                _alloc = value == NULL ? 0 : parseUnits(value, BYTES);

            CASE("lock")
                _lock = value == NULL ? 0 : parseUnits(value, NANOS);

            CASE("wall")
                _wall = value == NULL ? 0 : parseUnits(value, NANOS);

            CASE("cpu")
                if (_event != NULL) {
                    msg = "Duplicate event argument";
                } else {
                    _event = EVENT_CPU;
                }

            CASE("interval")
                if (value == NULL || (_interval = parseUnits(value, UNIVERSAL)) <= 0) {
                    msg = "Invalid interval";
                }

            CASE("jstackdepth")
                if (value == NULL || (_jstackdepth = atoi(value)) <= 0) {
                    msg = "jstackdepth must be > 0";
                }

            CASE("signal")
                if (value == NULL || (_signal = atoi(value)) <= 0) {
                    msg = "signal must be > 0";
                } else if ((value = strchr(value, '/')) != NULL) {
                    // Two signals were specified: one for CPU profiling, another for wall clock
                    _signal |= atoi(value + 1) << 8;
                }

            CASE("features")
                if (value != NULL) {
                    if (strstr(value, "probesp"))  _features.probe_sp = 1;
                    if (strstr(value, "vtable"))   _features.vtable_target = 1;
                    if (strstr(value, "comptask")) _features.comp_task = 1;
                    if (strstr(value, "pcaddr"))   _features.pc_addr = 1;
                }

            CASE("safemode") {
                // Left for compatibility purpose; will be eventually migrated to 'features'
                int bits = value == NULL ? INT_MAX : (int)strtol(value, NULL, 0);
                _features.unknown_java  = (bits & 1) ? 0 : 1;
                _features.unwind_stub   = (bits & 2) ? 0 : 1;
                _features.unwind_comp   = (bits & 4) ? 0 : 1;
                _features.unwind_native = (bits & 8) ? 0 : 1;
                _features.java_anchor   = (bits & 16) ? 0 : 1;
                _features.gc_traces     = (bits & 32) ? 0 : 1;
            }

            CASE("file")
                if (value == NULL || value[0] == 0) {
                    msg = "file must not be empty";
                }
                _file = value;

            CASE("log")
                _log = value == NULL || value[0] == 0 ? NULL : value;

            CASE("loglevel")
                if (value == NULL || value[0] == 0) {
                    msg = "loglevel must not be empty";
                }
                _loglevel = value;

            CASE("server")
                if (value == NULL || value[0] == 0) {
                    msg = "server address must not be empty";
                }
                _server = value;

            CASE("fdtransfer")
                _fdtransfer = true;
                if (value == NULL || value[0] == 0) {
                    msg = "fdtransfer path must not be empty";
                }
                _fdtransfer_path = value;

            // Filters
            CASE("filter")
                _filter = value == NULL ? "" : value;

            CASE("include")
                // Workaround -Wstringop-overflow warning
                if (value == arg + 8) appendToEmbeddedList(_include, arg + 8);

            CASE("exclude")
                // Workaround -Wstringop-overflow warning
                if (value == arg + 8) appendToEmbeddedList(_exclude, arg + 8);

            CASE("threads")
                _threads = true;

            CASE("sched")
                _sched = true;

            CASE("live")
                _live = true;

            CASE("allkernel")
                _ring = RING_KERNEL;

            CASE("alluser")
                _ring = RING_USER;

            CASE("cstack")
                if (value != NULL) {
                    switch (value[0]) {
                        case 'n': _cstack = CSTACK_NO;    break;
                        case 'd': _cstack = CSTACK_DWARF; break;
                        case 'l': _cstack = CSTACK_LBR;   break;
                        case 'v': _cstack = CSTACK_VM;    break;
                        default:  _cstack = CSTACK_FP;
                    }
                }

            CASE("clock")
                if (value != NULL) {
                    if (value[0] == 't') {
                        _clock = CLK_TSC;
                    } else if (value[0] == 'm') {
                        _clock = CLK_MONOTONIC;
                    }
                }

            // Output style modifiers
            CASE("simple")
                _style |= STYLE_SIMPLE;

            CASE("dot")
                _style |= STYLE_DOTTED;

            CASE("norm")
                _style |= STYLE_NORMALIZE;

            CASE("sig")
                _style |= STYLE_SIGNATURES;

            CASE("ann")
                _style |= STYLE_ANNOTATE;

            CASE("lib")
                _style |= STYLE_LIB_NAMES;

            CASE("mcache")
                _mcache = value == NULL ? 1 : (unsigned char)strtol(value, NULL, 0);

            CASE("begin")
                _begin = value;

            CASE("end")
                _end = value;

            // FlameGraph options
            CASE("title")
                _title = value;

            CASE("minwidth")
                if (value != NULL) _minwidth = atof(value);

            CASE("reverse")
                _reverse = true;

            DEFAULT()
                if (_unknown_arg == NULL) _unknown_arg = arg;
        }
    }

    // Return error only after parsing all arguments, when 'log' is already set
    if (msg != NULL) {
        return Error(msg);
    }

    if (_event == NULL && _alloc < 0 && _lock < 0 && _wall < 0) {
        _event = EVENT_CPU;
    }

    if (_file != NULL && _output == OUTPUT_NONE) {
        _output = detectOutputFormat(_file);
        if (_output == OUTPUT_SVG) {
            return Error("SVG format is obsolete, use .html for FlameGraph");
        }
        _dump_traces = 100;
        _dump_flat = 200;
    }

    if (_action == ACTION_NONE && _output != OUTPUT_NONE) {
        _action = ACTION_DUMP;
    }

    return Error::OK;
}

const char* Arguments::file() {
    if (_file != NULL && strchr(_file, '%') != NULL) {
        return expandFilePattern(_file);
    }
    return _file;
}

// Returns true if the log file is a temporary file of asprof launcher
bool Arguments::hasTemporaryLog() const {
    return _log != NULL && strncmp(_log, "/tmp/asprof-log.", 16) == 0;
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

// Expands the following patterns:
//   %p       process id
//   %t       timestamp (yyyyMMdd-hhmmss)
//   %n{MAX}  sequence number
//   %{ENV}   environment variable
const char* Arguments::expandFilePattern(const char* pattern) {
    char* ptr = _buf;
    char* end = _buf + EXTRA_BUF_SIZE - 1;

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
            } else if (c == 'n') {
                unsigned int max_files = 0;
                const char* p;
                if (*pattern == '{' && (p = strchr(pattern, '}')) != NULL) {
                    max_files = atoi(pattern + 1);
                    pattern = p + 1;
                }
                ptr += snprintf(ptr, end - ptr, "%u", max_files > 0 ? _file_num % max_files : _file_num);
                continue;
            } else if (c == '{') {
                char env_key[128];
                const char* p = strchr(pattern, '}');
                if (p != NULL && p - pattern < sizeof(env_key)) {
                    memcpy(env_key, pattern, p - pattern);
                    env_key[p - pattern] = 0;
                    const char* env_value = getenv(env_key);
                    if (env_value != NULL) {
                        ptr += snprintf(ptr, end - ptr, "%s", env_value);
                        pattern = p + 1;
                        continue;
                    }
                }
            }
        }
        *ptr++ = c;
    }

    *(ptr < end ? ptr : end) = 0;
    return _buf;
}

Output Arguments::detectOutputFormat(const char* file) {
    const char* ext = strrchr(file, '.');
    if (ext != NULL) {
        if (strcmp(ext, ".html") == 0) {
            return OUTPUT_FLAMEGRAPH;
        } else if (strcmp(ext, ".jfr") == 0) {
            return OUTPUT_JFR;
        } else if (strcmp(ext, ".collapsed") == 0 || strcmp(ext, ".folded") == 0) {
            return OUTPUT_COLLAPSED;
        } else if (strcmp(ext, ".svg") == 0) {
            return OUTPUT_SVG;
        }
    }
    return OUTPUT_TEXT;
}

long Arguments::parseUnits(const char* str, const Multiplier* multipliers) {
    char* end;
    long result = strtol(str, &end, 0);
    if (end == str) {
        return -1;
    }

    char c = *end;
    if (c == 0) {
        return result;
    }
    if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
    }

    for (const Multiplier* m = multipliers; m->symbol; m++) {
        if (c == m->symbol) {
            return result * m->multiplier;
        }
    }

    return -1;
}

int Arguments::parseTimeout(const char* str) {
    const char* p = strchr(str, ':');
    if (p == NULL) {
        return parseUnits(str, SECONDS);
    }

    int hh = str[0] >= '0' && str[0] <= '2' ? atoi(str) : 0xff;
    int mm = p[1] >= '0' && p[1] <= '5' ? atoi(p + 1) : 0xff;
    int ss = (p = strchr(p + 1, ':')) != NULL && p[1] >= '0' && p[1] <= '5' ? atoi(p + 1) : 0xff;
    return 0xff000000 | hh << 16 | mm << 8 | ss;
}

Arguments::~Arguments() {
    if (!_shared) free(_buf);
}

void Arguments::save() {
    if (this != &_global_args) {
        free(_global_args._buf);
        _global_args = *this;
        _shared = true;
    }
}
