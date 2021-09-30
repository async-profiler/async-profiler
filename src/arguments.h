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

#ifndef _ARGUMENTS_H
#define _ARGUMENTS_H

#include <stddef.h>


const long DEFAULT_INTERVAL = 10000000;  // 10 ms
const int DEFAULT_JSTACKDEPTH = 2048;

const char* const EVENT_CPU    = "cpu";
const char* const EVENT_ALLOC  = "alloc";
const char* const EVENT_LOCK   = "lock";
const char* const EVENT_WALL   = "wall";
const char* const EVENT_ITIMER = "itimer";

enum Action {
    ACTION_NONE,
    ACTION_START,
    ACTION_RESUME,
    ACTION_STOP,
    ACTION_DUMP,
    ACTION_CHECK,
    ACTION_STATUS,
    ACTION_LIST,
    ACTION_VERSION,
    ACTION_FULL_VERSION
};

enum Counter {
    COUNTER_SAMPLES,
    COUNTER_TOTAL
};

enum Ring {
    RING_ANY,
    RING_KERNEL,
    RING_USER
};

enum Style {
    STYLE_SIMPLE     = 1,
    STYLE_DOTTED     = 2,
    STYLE_SIGNATURES = 4,
    STYLE_ANNOTATE   = 8,
    STYLE_LIB_NAMES  = 16
};

enum CStack {
    CSTACK_DEFAULT,
    CSTACK_NO,
    CSTACK_FP,
    CSTACK_LBR
};

enum Output {
    OUTPUT_NONE,
    OUTPUT_TEXT,
    OUTPUT_SVG,  // obsolete
    OUTPUT_COLLAPSED,
    OUTPUT_FLAMEGRAPH,
    OUTPUT_TREE,
    OUTPUT_JFR
};

enum JfrOption {
    NO_SYSTEM_INFO  = 0x1,
    NO_SYSTEM_PROPS = 0x2,
    NO_NATIVE_LIBS  = 0x4,
    NO_CPU_LOAD     = 0x8,

    JFR_SYNC_OPTS   = NO_SYSTEM_INFO | NO_SYSTEM_PROPS | NO_NATIVE_LIBS | NO_CPU_LOAD
};


struct Multiplier {
    char symbol;
    long multiplier;
};


class Error {
  private:
    const char* _message;

  public:
    static const Error OK;

    explicit Error(const char* message) : _message(message) {
    }

    const char* message() {
        return _message;
    }

    operator bool() {
        return _message != NULL;
    }
};


class Arguments {
  private:
    char* _buf;
    bool _shared;

    void appendToEmbeddedList(int& list, char* value);

    static long long hash(const char* arg);
    static const char* expandFilePattern(char* dest, size_t max_size, const char* pattern);
    static Output detectOutputFormat(const char* file);
    static long parseUnits(const char* str, const Multiplier* multipliers);

  public:
    Action _action;
    Counter _counter;
    Ring _ring;
    const char* _event;
    long _interval;
    long _alloc;
    long _lock;
    int  _jstackdepth;
    int _safe_mode;
    const char* _file;
    const char* _log;
    const char* _filter;
    int _include;
    int _exclude;
    bool _threads;
    bool _sched;
    bool _fdtransfer;
    const char* _fdtransfer_path;
    int _style;
    CStack _cstack;
    Output _output;
    long _chunk_size;
    long _chunk_time;
    const char* _jfr_sync;
    int _jfr_options;
    int _dump_traces;
    int _dump_flat;
    const char* _begin;
    const char* _end;
    // FlameGraph parameters
    const char* _title;
    double _minwidth;
    bool _reverse;

    Arguments() :
        _buf(NULL),
        _shared(false),
        _action(ACTION_NONE),
        _counter(COUNTER_SAMPLES),
        _ring(RING_ANY),
        _event(NULL),
        _interval(0),
        _alloc(0),
        _lock(0),
        _jstackdepth(DEFAULT_JSTACKDEPTH),
        _safe_mode(0),
        _file(NULL),
        _log(NULL),
        _filter(NULL),
        _include(0),
        _exclude(0),
        _threads(false),
        _sched(false),
        _fdtransfer(false),
        _fdtransfer_path(NULL),
        _style(0),
        _cstack(CSTACK_DEFAULT),
        _output(OUTPUT_NONE),
        _chunk_size(100 * 1024 * 1024),
        _chunk_time(3600),
        _jfr_sync(NULL),
        _jfr_options(0),
        _dump_traces(0),
        _dump_flat(0),
        _begin(NULL),
        _end(NULL),
        _title(NULL),
        _minwidth(0),
        _reverse(false) {
    }

    ~Arguments();

    void save(Arguments& other);

    Error parse(const char* args);

    bool hasOutputFile() const {
        return _file != NULL &&
            (_action == ACTION_STOP || _action == ACTION_DUMP ? _output != OUTPUT_JFR : _action >= ACTION_STATUS);
    }

    bool hasOption(JfrOption option) const {
        return (_jfr_options & option) != 0;
    }

    friend class FrameName;
    friend class Recording;
};

#endif // _ARGUMENTS_H
