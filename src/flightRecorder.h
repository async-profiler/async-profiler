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
    // Preserve the next chunk's ID base across stop/resume.
    u64 _base_id;

    Error startMasterRecording(Arguments& args, const char* filename);
    void stopMasterRecording();

  public:
    static const LogLevel MIN_LOG_LEVEL = LogLevel::LOG_DEBUG;

    FlightRecorder() : _rec(NULL), _base_id(0) {
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

    static bool isJfrStarting();
};

#endif // _FLIGHTRECORDER_H
