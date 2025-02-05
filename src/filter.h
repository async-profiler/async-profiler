/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FILTER_H
#define _FILTER_H

#ifdef __linux__

class HeartBeatFilter {
private:
    int _fd;
    u64 *_region_ptr;
    int _clock_id;
    u64 _delay_ns;
    const char* _file_path;
public:
    HeartBeatFilter(const char* heartbeat_file, u64 delay_ns, bool use_unix_clock, bool use_realtime_clock);
  ~HeartBeatFilter();
    bool shouldProcess();

    HeartBeatFilter& operator = (const HeartBeatFilter&) = delete;
    HeartBeatFilter(const HeartBeatFilter&) = delete;
};

#else

class HeartBeatFilter {
public:
    HeartBeatFilter(const char* heartbeat_file, u64 delay_ns, bool use_unix_clock, bool use_realtime_clock) {
    }
    bool shouldProcess() {
        return true;
    }
};

#endif

#endif // _FILTER_H
