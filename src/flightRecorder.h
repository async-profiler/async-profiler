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

#include "arch.h"
#include "arguments.h"
#include "event.h"
#include "log.h"

class Recording;

class FlightRecorder {
  private:
    Recording* _rec;

    Error startMasterRecording(Arguments& args, const char* filename);
    void stopMasterRecording();

  public:
    FlightRecorder() : _rec(NULL) {
    }

    Error start(Arguments& args, bool reset);
    void stop();
    void flush();
    size_t usedMemory();
    bool timerTick(u64 wall_time);

    bool active() const {
        return _rec != NULL;
    }

    void recordEvent(int lock_index, int tid, u32 call_trace_id,
                     int event_type, Event* event, u64 counter);

    void recordLog(LogLevel level, const char* message, size_t len);
};

#endif // _FLIGHTRECORDER_H
