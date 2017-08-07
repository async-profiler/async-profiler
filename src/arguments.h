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


const int DEFAULT_INTERVAL = 1000000;  // 1 ms
const int DEFAULT_FRAMEBUF = 1000000;


enum Action {
    ACTION_NONE,
    ACTION_START,
    ACTION_STOP,
    ACTION_STATUS,
    ACTION_DUMP
};

enum Mode {
    MODE_CPU,
    MODE_HEAP
};

class Arguments {
  private:
    char _buf[1024];
    const char* _error;

    const char* parse(char* args);

  public:
    Action _action;
    Mode _mode;
    int _interval;
    int _framebuf;
    char* _file;
    bool _dump_collapsed;
    bool _dump_summary;
    int _dump_traces;
    int _dump_flat;

    Arguments(char* args) :
        _action(ACTION_NONE),
        _mode(MODE_CPU),
        _interval(DEFAULT_INTERVAL),
        _framebuf(DEFAULT_FRAMEBUF),
        _file(NULL),
        _dump_collapsed(false),
        _dump_summary(false),
        _dump_traces(0),
        _dump_flat(0) {
        _error = parse(args);
    }

    const char* error() { return _error; }
};

#endif // _ARGUMENTS_H
