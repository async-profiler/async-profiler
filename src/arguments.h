/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ARGUMENTS_H
#define _ARGUMENTS_H

#include <stddef.h>


const long DEFAULT_INTERVAL = 10000000;      // 10 ms
const long DEFAULT_ALLOC_INTERVAL = 524287;  // 512 KiB
const long DEFAULT_LOCK_INTERVAL = 10000;    // 10 us
const int DEFAULT_JSTACKDEPTH = 2048;

const char* const EVENT_CPU    = "cpu";
const char* const EVENT_ALLOC  = "alloc";
const char* const EVENT_LOCK   = "lock";
const char* const EVENT_WALL   = "wall";
const char* const EVENT_CTIMER = "ctimer";
const char* const EVENT_ITIMER = "itimer";

#define SHORT_ENUM __attribute__((__packed__))

enum SHORT_ENUM Action {
    ACTION_NONE,
    ACTION_START,
    ACTION_RESUME,
    ACTION_STOP,
    ACTION_DUMP,
    ACTION_CHECK,
    ACTION_STATUS,
    ACTION_MEMINFO,
    ACTION_LIST,
    ACTION_VERSION
};

enum SHORT_ENUM Counter {
    COUNTER_SAMPLES,
    COUNTER_TOTAL
};

enum Style {
    STYLE_SIMPLE       = 0x1,
    STYLE_DOTTED       = 0x2,
    STYLE_NORMALIZE    = 0x4,
    STYLE_SIGNATURES   = 0x8,
    STYLE_ANNOTATE     = 0x10,
    STYLE_LIB_NAMES    = 0x20,
    STYLE_NO_SEMICOLON = 0x40
};

// Whenever enum changes, update SETTING_CSTACK in FlightRecorder
enum SHORT_ENUM CStack {
    CSTACK_DEFAULT,
    CSTACK_NO,
    CSTACK_FP,
    CSTACK_DWARF,
    CSTACK_LBR,
    CSTACK_VM
};

enum SHORT_ENUM Clock {
    CLK_DEFAULT,
    CLK_TSC,
    CLK_MONOTONIC
};

enum SHORT_ENUM Output {
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
    NO_HEAP_SUMMARY = 0x10,

    IN_MEMORY       = 0x100,

    JFR_SYNC_OPTS   = NO_SYSTEM_INFO | NO_SYSTEM_PROPS | NO_NATIVE_LIBS | NO_CPU_LOAD | NO_HEAP_SUMMARY
};

struct StackWalkFeatures {
    // Stack recovery techniques used to workaround AsyncGetCallTrace flaws
    unsigned short unknown_java  : 1;
    unsigned short unwind_stub   : 1;
    unsigned short unwind_comp   : 1;
    unsigned short unwind_native : 1;
    unsigned short java_anchor   : 1;
    unsigned short gc_traces     : 1;

    // Additional HotSpot-specific features
    unsigned short probe_sp      : 1;
    unsigned short vtable_target : 1;
    unsigned short comp_task     : 1;
    unsigned short pc_addr       : 1;
    unsigned short _reserved     : 6;

    StackWalkFeatures() : unknown_java(1), unwind_stub(1), unwind_comp(1), unwind_native(1), java_anchor(1), gc_traces(1),
                          probe_sp(0), vtable_target(0), comp_task(0), pc_addr(0), _reserved(0) {
    }
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
    const char* expandFilePattern(const char* pattern);

    static long long hash(const char* arg);
    static Output detectOutputFormat(const char* file);
    static long parseUnits(const char* str, const Multiplier* multipliers);
    static int parseTimeout(const char* str);

  public:
    Action _action;
    Counter _counter;
    const char* _event;
    int _timeout;
    long _interval;
    long _alloc;
    long _lock;
    long _wall;
    int _jstackdepth;
    int _signal;
    const char* _file;
    const char* _log;
    const char* _loglevel;
    const char* _unknown_arg;
    const char* _server;
    const char* _filter;
    int _include;
    int _exclude;
    unsigned char _mcache;
    bool _loop;
    bool _preloaded;
    bool _threads;
    bool _sched;
    bool _live;
    bool _nobatch;
    bool _alluser;
    bool _fdtransfer;
    const char* _fdtransfer_path;
    int _style;
    StackWalkFeatures _features;
    CStack _cstack;
    Clock _clock;
    Output _output;
    long _chunk_size;
    long _chunk_time;
    const char* _jfr_sync;
    int _jfr_options;
    int _dump_traces;
    int _dump_flat;
    unsigned int _file_num;
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
        _event(NULL),
        _timeout(0),
        _interval(0),
        _alloc(-1),
        _lock(-1),
        _wall(-1),
        _jstackdepth(DEFAULT_JSTACKDEPTH),
        _signal(0),
        _file(NULL),
        _log(NULL),
        _loglevel(NULL),
        _unknown_arg(NULL),
        _server(NULL),
        _filter(NULL),
        _include(0),
        _exclude(0),
        _mcache(0),
        _loop(false),
        _preloaded(false),
        _threads(false),
        _sched(false),
        _live(false),
        _nobatch(false),
        _alluser(false),
        _fdtransfer(false),
        _fdtransfer_path(NULL),
        _style(0),
        _features(),
        _cstack(CSTACK_DEFAULT),
        _clock(CLK_DEFAULT),
        _output(OUTPUT_NONE),
        _chunk_size(100 * 1024 * 1024),
        _chunk_time(3600),
        _jfr_sync(NULL),
        _jfr_options(0),
        _dump_traces(0),
        _dump_flat(0),
        _file_num(0),
        _begin(NULL),
        _end(NULL),
        _title(NULL),
        _minwidth(0),
        _reverse(false) {
    }

    ~Arguments();

    void save();

    Error parse(const char* args);

    const char* file();

    bool hasTemporaryLog() const;

    bool hasOutputFile() const {
        return _file != NULL &&
            (_action == ACTION_STOP || _action == ACTION_DUMP ? _output != OUTPUT_JFR : _action >= ACTION_CHECK);
    }

    bool hasOption(JfrOption option) const {
        return (_jfr_options & option) != 0;
    }

    friend class FrameName;
    friend class Recording;
};

extern Arguments _global_args;

#endif // _ARGUMENTS_H
