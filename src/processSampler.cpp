/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "processSampler.h"
#include <unordered_set>

const float MIN_CPU_THRESHOLD = 0.05f;  // Minimum 5% cpu utilization to include results
const float MIN_RSS_THRESHOLD = 0.05f;  // Minimum 5% rss usage to include results

u64 ProcessSampler::_last_sample_time = 0;
std::unordered_map<int, ProcessHistory> ProcessSampler::_process_history;

double ProcessSampler::getRssUsageRatio(const ProcessInfo& info) {
    const u64 ram_size = OS::getRamSize();
    if (ram_size == 0 || info.vm_rss == 0) return 0.0;

    return (double)info.vm_rss / ram_size;
}

bool ProcessSampler::shouldIncludeProcess(const ProcessInfo& info) {
    return info.cpu_percent >= MIN_CPU_THRESHOLD || getRssUsageRatio(info) >= MIN_RSS_THRESHOLD;
}

bool ProcessSampler::populateCpuPercent(ProcessInfo& info, const u64 sampling_time) {
    const float current_cpu_total = info.cpu_user + info.cpu_system;
    ProcessHistory& history = _process_history[info.pid];
    if (history.prev_timestamp == 0 || history.start_time != info.start_time) {
        history.prev_cpu_total = current_cpu_total;
        history.prev_timestamp = sampling_time;
        history.start_time = info.start_time;
        return false;
    }

    const float delta_cpu = current_cpu_total - history.prev_cpu_total;
    const u64 delta_time = sampling_time - history.prev_timestamp;
    info.cpu_percent = delta_cpu * 1e9f / delta_time;

    history.prev_cpu_total = current_cpu_total;
    history.prev_timestamp = sampling_time;
    return true;
}

int ProcessSampler::sample(u64 wall_time) {
    const int pid_count = OS::getProcessIds(_pids, MAX_PROCESSES);
    cleanupProcessHistory(pid_count);
    _last_sample_time = wall_time;
    return pid_count;
}

void ProcessSampler::cleanupProcessHistory(const int pid_count) {
    const std::unordered_set<int> active_pid_set(_pids, _pids + pid_count);
    for (auto it = _process_history.begin(); it != _process_history.end();) {
        if (active_pid_set.count(it->first) == 0) {
            it = _process_history.erase(it);
        } else {
            ++it;
        }
    }
}

void ProcessSampler::enable(const long sampling_interval) {
    if (_sampling_interval != sampling_interval) {
        _sampling_interval = sampling_interval;
        _last_sample_time = 0;
        _process_history.clear();
    }
}

bool ProcessSampler::shouldSample(const u64 wall_time) const {
    return _sampling_interval > 0 && wall_time >= _last_sample_time + _sampling_interval;
}

bool ProcessSampler::getProcessInfo(int pid_index, u64 sampling_time, ProcessInfo& info) {
    const int pid = _pids[pid_index];
    return OS::getBasicProcessInfo(pid, &info) && populateCpuPercent(info, sampling_time) &&
           shouldIncludeProcess(info) && OS::getDetailedProcessInfo(&info);
}