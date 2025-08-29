/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
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
    bool timerTick(u64 wall_time, u32 gc_id);

    bool active() const {
        return _rec != NULL;
    }

    void recordEvent(int lock_index, int tid, u32 call_trace_id,
                     EventType event_type, Event* event);

    void recordLog(LogLevel level, const char* message, size_t len);
};

#endif // _FLIGHTRECORDER_H
