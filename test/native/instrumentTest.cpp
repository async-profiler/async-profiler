/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "testRunner.hpp"
#include "instrument.h"

TEST_CASE(Instrument_test_updateCurrentFrame_start) {
    long current_frame_old = -1;
    long current_frame_new = -1;
    u32 relocation_table[3];
    relocation_table[2] = 4;
    u16 offset_delta_old = 2;

    u16 offset_delta_new = updateCurrentFrame(current_frame_old, current_frame_new, offset_delta_old, relocation_table);
    CHECK_EQ(current_frame_old, 2);
    CHECK_EQ(current_frame_new, 6);
    CHECK_EQ(offset_delta_new, 6);
}

TEST_CASE(Instrument_test_updateCurrentFrame_newEntry) {
    long current_frame_old = -1;
    long current_frame_new = 4;
    u32 relocation_table[3];
    relocation_table[2] = 4;
    u16 offset_delta_old = 2;

    u32 offset_delta_new = updateCurrentFrame(current_frame_old, current_frame_new, offset_delta_old, relocation_table);
    CHECK_EQ(current_frame_old, 2);
    CHECK_EQ(current_frame_new, 6);
    // offset from first StackMapTable entry to the next
    CHECK_EQ(offset_delta_new, 1);
}

TEST_CASE(Instrument_test_updateCurrentFrame_mid) {
    long current_frame_old = 1;
    long current_frame_new = 2;
    u32 relocation_table[3];
    relocation_table[2] = 4;
    u16 offset_delta_old = 0;

    u32 offset_delta_new = updateCurrentFrame(current_frame_old, current_frame_new, offset_delta_old, relocation_table);
    CHECK_EQ(current_frame_old, 2);
    CHECK_EQ(current_frame_new, 6);
    CHECK_EQ(offset_delta_new, 3);
}

TEST_CASE(Instrument_test_countParametersSlots_reference) {
    CHECK_EQ(countParametersSlots("(Ljava/time/Duration;)"), 1);
}

TEST_CASE(Instrument_test_countParametersSlots_referenceArray) {
    CHECK_EQ(countParametersSlots("([Ljava/time/Duration;)"), 1);
}

TEST_CASE(Instrument_test_countParametersSlots_2darray) {
    CHECK_EQ(countParametersSlots("([[I)"), 1);
}

TEST_CASE(Instrument_test_countParametersSlots_2slots) {
    CHECK_EQ(countParametersSlots("(D)"), 2);
    CHECK_EQ(countParametersSlots("(J)"), 2);
}

TEST_CASE(Instrument_test_countParametersSlots_1slot) {
    CHECK_EQ(countParametersSlots("(Z)"), 1);
    CHECK_EQ(countParametersSlots("(I)"), 1);
    CHECK_EQ(countParametersSlots("(F)"), 1);
}

TEST_CASE(Instrument_test_countParametersSlots_doubleArray) {
    CHECK_EQ(countParametersSlots("([D)"), 1);
}

TEST_CASE(Instrument_test_countParametersSlots_mix) {
    CHECK_EQ(countParametersSlots("(ZD[I)"), 4);
}
