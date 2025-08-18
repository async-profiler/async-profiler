/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "testRunner.hpp"
#include "classfile_constants.h"
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

TEST_CASE(Instrument_test_parametersSlots_reference) {
    CHECK_EQ(parametersSlots("(Ljava/time/Duration;)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_referenceArray) {
    CHECK_EQ(parametersSlots("([Ljava/time/Duration;)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_2darray) {
    CHECK_EQ(parametersSlots("([[I)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_2slots) {
    CHECK_EQ(parametersSlots("(D)"), 2);
    CHECK_EQ(parametersSlots("(J)"), 2);
}

TEST_CASE(Instrument_test_parametersSlots_1slot) {
    CHECK_EQ(parametersSlots("(Z)"), 1);
    CHECK_EQ(parametersSlots("(I)"), 1);
    CHECK_EQ(parametersSlots("(F)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_doubleArray) {
    CHECK_EQ(parametersSlots("([D)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_mix) {
    CHECK_EQ(parametersSlots("(ZD[I)"), 4);
}

TEST_CASE(Instrument_test_computeInstructionByteCount) {
    u8 code[1];
    code[0] = JVM_OPC_invokevirtual;
    CHECK_EQ(computeInstructionByteCount(code, 0), 3);

    code[0] = JVM_OPC_istore;
    CHECK_EQ(computeInstructionByteCount(code, 0), 2);
    
    code[0] = JVM_OPC_istore_2;
    CHECK_EQ(computeInstructionByteCount(code, 0), 1);
}

TEST_CASE(Instrument_test_computeInstructionByteCount_lookupswitch) {
    u8 code[12];
    code[0] = JVM_OPC_lookupswitch;
    code[11] = 3; // pairs
    CHECK_EQ(computeInstructionByteCount(code, 0), 36);
}

TEST_CASE(Instrument_test_computeInstructionByteCount_tableswitch) {
    u8 code[16];
    code[0] = JVM_OPC_tableswitch;
    code[11] = 3; // low
    code[15] = 10; // high
    CHECK_EQ(computeInstructionByteCount(code, 0), 48);
}
