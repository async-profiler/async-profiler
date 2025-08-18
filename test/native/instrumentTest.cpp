/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include "testRunner.hpp"
#include "classfile_constants.h"
#include "instrument.h"

TEST_CASE(Instrument_test_updateCurrentFrame_start) {
    long current_frame_old = -1;
    long current_frame_new = -1;
    u16 relocation_table[3];
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
    u16 relocation_table[3];
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
    u16 relocation_table[3];
    relocation_table[2] = 4;
    u16 offset_delta_old = 0;

    u32 offset_delta_new = updateCurrentFrame(current_frame_old, current_frame_new, offset_delta_old, relocation_table);
    CHECK_EQ(current_frame_old, 2);
    CHECK_EQ(current_frame_new, 6);
    CHECK_EQ(offset_delta_new, 3);
}

TEST_CASE(Instrument_test_parametersSlots_reference) {
    CHECK_EQ(parameterSlots("(Ljava/time/Duration;)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_referenceArray) {
    CHECK_EQ(parameterSlots("([Ljava/time/Duration;)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_2darray) {
    CHECK_EQ(parameterSlots("([[I)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_2slots) {
    CHECK_EQ(parameterSlots("(D)"), 2);
    CHECK_EQ(parameterSlots("(J)"), 2);
}

TEST_CASE(Instrument_test_parametersSlots_1slot) {
    CHECK_EQ(parameterSlots("(Z)"), 1);
    CHECK_EQ(parameterSlots("(I)"), 1);
    CHECK_EQ(parameterSlots("(F)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_doubleArray) {
    CHECK_EQ(parameterSlots("([D)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_mix) {
    CHECK_EQ(parameterSlots("(ZD[I)"), 4);
}

TEST_CASE(Instrument_test_instructionBytes) {
    u8 code[1];
    code[0] = JVM_OPC_invokevirtual;
    CHECK_EQ(instructionBytes(code, 0), 3);

    code[0] = JVM_OPC_istore;
    CHECK_EQ(instructionBytes(code, 0), 2);
    
    code[0] = JVM_OPC_istore_2;
    CHECK_EQ(instructionBytes(code, 0), 1);
}

TEST_CASE(Instrument_test_instructionBytes_lookupswitch) {
    u8 code[12];
    code[0] = JVM_OPC_lookupswitch;
    *(u32*)(code+8) = htonl(3); // pairs
    CHECK_EQ(instructionBytes(code, 0), 12 + 3 * 4 * 2);
}

TEST_CASE(Instrument_test_instructionBytes_largeLookupswitch) {
    u8 code[12];
    code[0] = JVM_OPC_lookupswitch;
    *(u32*)(code+8) = htonl(0xFFFFFF); // pairs
    CHECK_EQ(instructionBytes(code, 0), 12 + 0xFFFFFF * 4 * 2);
}

TEST_CASE(Instrument_test_instructionBytes_tableswitch) {
    u8 code[16];
    code[0] = JVM_OPC_tableswitch;
    *(u32*)(code+8) = htonl(3); // pairs
    *(u32*)(code+12) = htonl(10); // pairs
    CHECK_EQ(instructionBytes(code, 0), 16 + 8 * 4);
}
