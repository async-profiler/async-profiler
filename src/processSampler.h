/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PROCESSSAMPLER_H
#define _PROCESSSAMPLER_H

#include <unordered_map>
#include "os.h"

const u64 MAX_TIME_NS = 900000000UL; // Timeout after 900ms to guarantee runtime <1sec
const int MAX_PROCESSES = 5000;      // Hard limit to prevent excessive work
const int MAX_PROCESS_SAMPLE_JFR_EVENT_LENGTH = 2500;

struct ProcessHistory {
    float prev_cpu_total = 0.0f;
    u64 prev_timestamp = 0;
    u64 start_time = 0;
};

class ProcessSampler {
  private:
    static u64 _last_sample_time;
    static std::unordered_map<int, ProcessHistory> _process_history;

    long _sampling_interval = -1;
    int _pids[MAX_PROCESSES];

    static double getRssUsageRatio(const ProcessInfo& info);
    static bool shouldIncludeProcess(const ProcessInfo& info);
    static bool populateCpuPercent(ProcessInfo& info, u64 sampling_time);

    void cleanupProcessHistory(int pid_count);

  public:
    void enable(long sampling_interval);
    bool shouldSample(u64 wall_time) const;
    int sample(u64 wall_time);
    bool getProcessInfo(int pid_index, u64 sampling_time, ProcessInfo& info);
};

#endif // _PROCESSSAMPLER_H
