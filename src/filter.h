/*
* Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FILTER_H
#define _FILTER_H

#ifdef __linux__

class HeartBitFilter {
private:
    int _fd;
    u64 *_region_ptr;
    int _clock_id;
    u64 _delay_ns;
    const char* _file_path;
public:
    HeartBitFilter(const char* heartbit_file, u64 delay_ns, bool use_unix_clock, bool use_realtime_clock);
  ~HeartBitFilter();
    bool shouldProcess();

    HeartBitFilter& operator = (const HeartBitFilter&) = delete;
    HeartBitFilter(const HeartBitFilter&) = delete;
};

#else

class HeartBitFilter {
public:
    HeartBitFilter(const char* heartbit_file, u64 delay_ns, bool use_unix_clock, bool use_realtime_clock) {
    }
    bool shouldProcess() {
        return true;
    }
};

#endif

#endif // _FILTER_H
