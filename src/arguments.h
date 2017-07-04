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

class Arguments {
  private:
    char _buf[1024];
    const char* _error;

    const char* parse(char* args);

  public:
    int _interval;
    int _framebuf;
    char* _file;
    Action _action;
    bool _dump_flamegraph;
    bool _dump_summary;
    int _dump_traces;
    int _dump_methods;

    Arguments(char* args) :
        _interval(DEFAULT_INTERVAL),
        _framebuf(DEFAULT_FRAMEBUF),
        _file(NULL),
        _action(ACTION_NONE),
        _dump_flamegraph(false),
        _dump_summary(false),
        _dump_traces(0),
        _dump_methods(0) {
        _error = parse(args);
    }

    const char* error() { return _error; }
};

#endif // _ARGUMENTS_H
