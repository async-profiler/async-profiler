/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */


#ifdef __linux__

#include <time.h>
#include <error.h>

#include "log.h"
#include "filter.h"

const u64 NS_IN_1_SEC = 1000000000;

int clock_id(bool use_realtime, bool use_unix) {
    if (use_realtime) {
        return CLOCK_REALTIME;
    } else if (use_unix) {
        return CLOCK_MONOTONIC;
    } else {
        Log::error("No clockid set");
  	return -1;
    }
}

HeartBitFilter::~HeartBitFilter() {
    munmap(this->_region_ptr, sizeof(u64));
    close(this->_fd);  
}

HeartBitFilter::HeartBitFilter(const char* heartbit_file, u64 interval_ns, bool use_unix_clock, bool use_realtime_clock) {
    this->_clock_id = clock_id(use_realtime_clock, use_unix_clock);
    this->_delay_ns = interval_ns;
    this->_file_path = heartbit_file;
    this->_fd = open(this->_file_path, O_RDONLY);
    if (this->_fd <= 0) {
        printf("Can't open heartbit file %s, %s", this->_file_path, strerror(errno));
    }
    this->_region_ptr = (u64*)mmap(NULL, sizeof(u64), PROT_READ, MAP_SHARED, this->_fd, 0);
    if (this->_region_ptr == MAP_FAILED) {
        printf("Can't mmap file file %s, %s\n", this->_file_path, strerror(errno));
    }
}

bool HeartBitFilter::shouldProcess() {
    u64 last_heartbit = *(u64*)this->_region_ptr;
    u64 profile_after = last_heartbit + this->_delay_ns;

    struct timespec nano_time = {};
    int res = clock_gettime(this->_clock_id, &nano_time);
    if (res) {
	Log::error("Can't get time. error: %s", strerror(errno));
    }

    u64 full_ts = (nano_time.tv_sec * NS_IN_1_SEC) + nano_time.tv_nsec;

    return full_ts >= profile_after;
}

#endif // __linix__
