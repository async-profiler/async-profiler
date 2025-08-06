/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "testRunner.hpp"
#include "profiler.h"
#include "safeAccess.h"

TEST_CASE(SafeAccess_load) {
    Profiler::instance()->updateSymbols(false);
    Profiler::instance()->setupSignalHandlers();

    void* good_val = (void*)(uintptr_t)0x123456789abcdef0ULL;
    void** good_ptr = &good_val;
    void** bad_ptr = (void**)(uintptr_t)0xbaad0000ffffbaadULL;
    void* deadbeef = (void*)(uintptr_t)0xdeadbeefdeadbeefULL;

    ASSERT_EQ(SafeAccess::load(bad_ptr), 0);
    ASSERT_EQ(SafeAccess::load(good_ptr), good_val);

    ASSERT_EQ(SafeAccess::load(bad_ptr, deadbeef), deadbeef);
    ASSERT_EQ(SafeAccess::load(good_ptr, deadbeef), good_val);

    ASSERT_EQ(SafeAccess::load32((int32_t*)bad_ptr, 0x87654321), 0x87654321);
    ASSERT_EQ(SafeAccess::load32((int32_t*)good_ptr, 0x87654321), 0x9abcdef0);
}
