/*
 * Copyright 2018 Andrei Pangin
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

#ifndef _FLIGHTRECORDER_H
#define _FLIGHTRECORDER_H

#include "arguments.h"


class Recording;

class FlightRecorder {
  private:
    Recording* _rec;

  public:
    FlightRecorder() : _rec(NULL) {
    }

    Error start(const char* file);
    void stop();

    void recordExecutionSample(int lock_index, int tid, int call_trace_id);
};

#endif // _FLIGHTRECORDER_H
