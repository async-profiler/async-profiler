/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>

#include "os.h"
#include "testRunner.hpp"

TEST_CASE(ProcessInfo_initialization) {
    ProcessInfo info;
    ASSERT_EQ(info.pid, 0);
    ASSERT_EQ(info.ppid, 0);
    ASSERT_EQ(info.cpu_percent, 0.0f);
    ASSERT_EQ(info.uid, 0U);
    ASSERT_EQ(info.state, 0);
    ASSERT_EQ(info.start_time, 0UL);
    ASSERT_EQ(info.cpu_user, 0UL);
    ASSERT_EQ(info.cpu_system, 0UL);
    ASSERT_EQ(info.threads, 0);
    ASSERT_EQ(info.vm_size, 0UL);
    ASSERT_EQ(info.vm_rss, 0UL);
    ASSERT_EQ(info.rss_anon, 0UL);
    ASSERT_EQ(info.rss_files, 0UL);
    ASSERT_EQ(info.rss_shmem, 0UL);
    ASSERT_EQ(info.minor_faults, 0UL);
    ASSERT_EQ(info.major_faults, 0UL);
    ASSERT_EQ(info.io_read, 0UL);
    ASSERT_EQ(info.io_write, 0UL);
}

#ifdef __linux__

TEST_CASE(GetProcessIds_returns_valid_pids) {
    int pids[100];
    int count = OS::getProcessIds(pids, 100);
    ASSERT_GT(count, 1);
    ASSERT_LTE(count, 100);

    for (int i = 0; i < count; i++) {
        ASSERT_GT(pids[i], 0);
    }

    bool found_init = false;
    for (int i = 0; i < count; i++) {
        if (pids[i] == 1) {
            found_init = true;
            break;
        }
    }
    ASSERT_EQ(found_init, true);
}

TEST_CASE(GetProcessIds_respects_max_limit) {
    int pids[5];
    int count = OS::getProcessIds(pids, 5);
    ASSERT_LTE(count, 5);
    ASSERT_GT(count, 0);
}

TEST_CASE(GetBasicProcessInfo_self_process) {
    ProcessInfo info;
    int self_pid = getpid();
    bool result = OS::getBasicProcessInfo(self_pid, &info);

    ASSERT_EQ(result, true);
    ASSERT_EQ(info.pid, self_pid);
    ASSERT_GT(info.ppid, 0);
    ASSERT_GT(info.threads, 0);
    ASSERT_GT(info.vm_size, 0UL);
    ASSERT_GT(info.vm_rss, 0UL);
    ASSERT_GTE(info.cpu_user, 0UL);
    ASSERT_GTE(info.cpu_system, 0UL);
    ASSERT_GTE(info.minor_faults, 0UL);
    ASSERT_GTE(info.major_faults, 0UL);
    ASSERT_GT(info.start_time, 0UL);
    ASSERT_NE(info.state, 0);
    ASSERT_NE(info.name[0], '\0');
}

TEST_CASE(GetBasicProcessInfo_invalid_pid) {
    ProcessInfo info;
    bool result = OS::getBasicProcessInfo(-1, &info);
    ASSERT_EQ(result, false);
}

TEST_CASE(GetBasicProcessInfo_nonexistent_pid) {
    ProcessInfo info;
    bool result = OS::getBasicProcessInfo(999999, &info);
    ASSERT_EQ(result, false);
}

TEST_CASE(GetDetailedProcessInfo_self_process) {
    ProcessInfo info;
    int self_pid = getpid();

    bool basic_result = OS::getBasicProcessInfo(self_pid, &info);
    ASSERT_EQ(basic_result, true);

    bool detailed_result = OS::getDetailedProcessInfo(&info);
    ASSERT_EQ(detailed_result, true);

    ASSERT_GTE(info.uid, 0U);
    ASSERT_GTE(info.rss_anon, 0UL);
    ASSERT_GTE(info.rss_files, 0UL);
    ASSERT_GTE(info.rss_shmem, 0UL);
    ASSERT_NE(info.cmdline[0], '\0');
}

TEST_CASE(GetSysBootTime_returns_valid_time) {
    u64 boot_time = OS::getSystemBootTime();
    ASSERT_GT(boot_time, 0UL);

    u64 year_2000 = 946684800UL; // Jan 1, 2000 UTC
    u64 current_time = time(NULL);
    ASSERT_GT(boot_time, year_2000);
    ASSERT_LT(boot_time, current_time);
}

TEST_CASE(ClockTicksPerSec_is_valid) {
    ASSERT_GT(OS::clock_ticks_per_sec, 0L);
    ASSERT_LTE(OS::clock_ticks_per_sec, 10000L);
}

#else

TEST_CASE(ProcessInfo_functions_not_implemented_on_non_linux) {
    int pids[10];
    int count = OS::getProcessIds(pids, 10);
    ASSERT_EQ(count, 0);

    ProcessInfo info;
    bool result = OS::getBasicProcessInfo(getpid(), &info);
    ASSERT_EQ(result, false);

    result = OS::getDetailedProcessInfo(&info);
    ASSERT_EQ(result, false);

    u64 boot_time = OS::getSystemBootTime();
    ASSERT_EQ(boot_time, 0UL);
}

#endif
