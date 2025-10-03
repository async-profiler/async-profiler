/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include <arpa/inet.h>
#include "testRunner.hpp"
#include "classfile_constants.h"
#include "instrument.h"

TEST_CASE(Instrument_test_updateCurrentFrame_start) {
    int32_t current_frame_old = -1;
    int32_t current_frame_new = -1;
    u16 relocation_table[3];
    relocation_table[2] = 4;
    u16 offset_delta_old = 2;

    u16 offset_delta_new = BytecodeRewriter::updateCurrentFrame(current_frame_old, current_frame_new, offset_delta_old, relocation_table);
    CHECK_EQ(current_frame_old, 2);
    CHECK_EQ(current_frame_new, 6);
    CHECK_EQ(offset_delta_new, 6);
}

TEST_CASE(Instrument_test_updateCurrentFrame_newEntry) {
    int32_t current_frame_old = -1;
    int32_t current_frame_new = 4;
    u16 relocation_table[3];
    relocation_table[2] = 4;
    u16 offset_delta_old = 2;

    u16 offset_delta_new = BytecodeRewriter::updateCurrentFrame(current_frame_old, current_frame_new, offset_delta_old, relocation_table);
    CHECK_EQ(current_frame_old, 2);
    CHECK_EQ(current_frame_new, 6);
    // offset from first StackMapTable entry to the next
    CHECK_EQ(offset_delta_new, 1);
}

TEST_CASE(Instrument_test_updateCurrentFrame_mid) {
    int32_t current_frame_old = 1;
    int32_t current_frame_new = 2;
    u16 relocation_table[3];
    relocation_table[2] = 4;
    u16 offset_delta_old = 0;

    u16 offset_delta_new = BytecodeRewriter::updateCurrentFrame(current_frame_old, current_frame_new, offset_delta_old, relocation_table);
    CHECK_EQ(current_frame_old, 2);
    CHECK_EQ(current_frame_new, 6);
    CHECK_EQ(offset_delta_new, 3);
}

TEST_CASE(Instrument_test_parametersSlots_reference) {
    CHECK_EQ(Constant::parameterSlots("(Ljava/time/Duration;)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_referenceArray) {
    CHECK_EQ(Constant::parameterSlots("([Ljava/time/Duration;)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_2darray) {
    CHECK_EQ(Constant::parameterSlots("([[I)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_2slots) {
    CHECK_EQ(Constant::parameterSlots("(D)"), 2);
    CHECK_EQ(Constant::parameterSlots("(J)"), 2);
}

TEST_CASE(Instrument_test_parametersSlots_1slot) {
    CHECK_EQ(Constant::parameterSlots("(Z)"), 1);
    CHECK_EQ(Constant::parameterSlots("(I)"), 1);
    CHECK_EQ(Constant::parameterSlots("(F)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_doubleArray) {
    CHECK_EQ(Constant::parameterSlots("([D)"), 1);
}

TEST_CASE(Instrument_test_parametersSlots_mix) {
    CHECK_EQ(Constant::parameterSlots("(ZD[I)"), 4);
}

TEST_CASE(Instrument_test_instructionBytes) {
    u8 code[1];
    code[0] = JVM_OPC_invokevirtual;
    CHECK_EQ(BytecodeRewriter::instructionBytes(code, 0), 3);

    code[0] = JVM_OPC_istore;
    CHECK_EQ(BytecodeRewriter::instructionBytes(code, 0), 2);
    
    code[0] = JVM_OPC_istore_2;
    CHECK_EQ(BytecodeRewriter::instructionBytes(code, 0), 1);
}

TEST_CASE(Instrument_test_instructionBytes_lookupswitch) {
    u8 code[12];
    code[0] = JVM_OPC_lookupswitch;

    u32 pairs = 3;
    *(u32*)(code+8) = htonl(pairs);

    CHECK_EQ(BytecodeRewriter::instructionBytes(code, 0), 12 + pairs * 4 * 2);
}

TEST_CASE(Instrument_test_instructionBytes_largeLookupswitch) {
    u8 code[12];
    code[0] = JVM_OPC_lookupswitch;

    u32 pairs = 0xFFF;
    *(u32*)(code+8) = htonl(pairs);

    CHECK_EQ(BytecodeRewriter::instructionBytes(code, 0), 12 + pairs * 4 * 2);
}

TEST_CASE(Instrument_test_instructionBytes_tableswitch) {
    u8 code[16];
    code[0] = JVM_OPC_tableswitch;

    int32_t low = 3;
    *(u32*)(code+8) = htonl(low);
    int32_t high = 10;
    *(u32*)(code+12) = htonl(high);

    CHECK_EQ(BytecodeRewriter::instructionBytes(code, 0), 16 + (high - low + 1) * 4);
}

TEST_CASE(Instrument_test_instructionBytes_largeTableswitch) {
    u8 code[16];
    code[0] = JVM_OPC_tableswitch;
    
    int32_t low = 0;
    *(u32*)(code+8) = htonl(low);
    int32_t high = 0xFFF;
    *(u32*)(code+12) = htonl(high);

    CHECK_EQ(BytecodeRewriter::instructionBytes(code, 0), 16 + (high - low + 1) * 4);
}

TEST_CASE(Instrument_test_handleTarget_default) {
    Targets t;
    Error e = handleTarget(t, "my.pkg.ClassName.MethodName", -28);
    CHECK_EQ(e.message(), NULL);
    CHECK_EQ(t.size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"].size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"]["MethodName"].size(), 1);

    const MethodTarget& target = t["my/pkg/ClassName"]["MethodName"][0];
    CHECK_EQ(target.signature.compare("*"), 0);
    CHECK_EQ(target.latency, -28);
}

TEST_CASE(Instrument_test_handleTarget_latency) {
    Targets t;
    Error e = handleTarget(t, "my.pkg.ClassName.MethodName:20ns", MethodTarget::NO_LATENCY);
    CHECK_EQ(e.message(), NULL);
    CHECK_EQ(t.size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"].size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"]["MethodName"].size(), 1);

    const MethodTarget& target = t["my/pkg/ClassName"]["MethodName"][0];
    CHECK_EQ(target.signature.compare("*"), 0);
    CHECK_EQ(target.latency, 20);
}

TEST_CASE(Instrument_test_handleTarget_signatureAndLatency) {
    Targets t;
    Error e = handleTarget(t, "my.pkg.ClassName.MethodName(Ljava/time/Duration;)V:20ns", MethodTarget::NO_LATENCY);
    CHECK_EQ(e.message(), NULL);
    CHECK_EQ(t.size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"].size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"]["MethodName"].size(), 1);

    const MethodTarget& target = t["my/pkg/ClassName"]["MethodName"][0];
    CHECK_EQ(target.signature.compare("(Ljava/time/Duration;)V"), 0);
    CHECK_EQ(target.latency, 20);
}

TEST_CASE(Instrument_test_handleTarget_manyClasses) {
    Targets t;
    Error e = handleTarget(t, "my.pkg.ClassName.MethodName(Ljava/time/Duration;)V:20ns", MethodTarget::NO_LATENCY);
    CHECK_EQ(e.message(), NULL);
    e = handleTarget(t, "my.pkg.AnotherClass.MethodName()V:100ms", MethodTarget::NO_LATENCY);
    CHECK_EQ(e.message(), NULL);

    CHECK_EQ(t.size(), 2);
    CHECK_EQ(t["my/pkg/ClassName"].size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"]["MethodName"].size(), 1);

    const MethodTarget& target = t["my/pkg/ClassName"]["MethodName"][0];
    CHECK_EQ(target.signature.compare("(Ljava/time/Duration;)V"), 0);
    CHECK_EQ(target.latency, 20);

    CHECK_EQ(t["my/pkg/AnotherClass"].size(), 1);
    CHECK_EQ(t["my/pkg/AnotherClass"]["MethodName"].size(), 1);

    const MethodTarget& another_target = t["my/pkg/AnotherClass"]["MethodName"][0];
    CHECK_EQ(another_target.signature.compare("()V"), 0);
    CHECK_EQ(another_target.latency, 100000000);
}

TEST_CASE(Instrument_test_handleTarget_manyMethods) {
    Targets t;
    Error e = handleTarget(t, "my.pkg.ClassName.MethodName(Ljava/time/Duration;)V:20ns", MethodTarget::NO_LATENCY);
    CHECK_EQ(e.message(), NULL);
    e = handleTarget(t, "my.pkg.ClassName.AnotherMethod()V:100ms", MethodTarget::NO_LATENCY);
    CHECK_EQ(e.message(), NULL);

    CHECK_EQ(t.size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"].size(), 2);
    CHECK_EQ(t["my/pkg/ClassName"]["MethodName"].size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"]["AnotherMethod"].size(), 1);

    const MethodTarget& target = t["my/pkg/ClassName"]["MethodName"][0];
    CHECK_EQ(target.signature.compare("(Ljava/time/Duration;)V"), 0);
    CHECK_EQ(target.latency, 20);

    const MethodTarget& another_target = t["my/pkg/ClassName"]["AnotherMethod"][0];
    CHECK_EQ(another_target.signature.compare("()V"), 0);
    CHECK_EQ(another_target.latency, 100000000);
}

TEST_CASE(Instrument_test_handleTarget_manySignatures) {
    Targets t;
    Error e = handleTarget(t, "my.pkg.ClassName.MethodName(Ljava/time/Duration;)V:20ns", MethodTarget::NO_LATENCY);
    CHECK_EQ(e.message(), NULL);
    e = handleTarget(t, "my.pkg.ClassName.MethodName()V:100ms", MethodTarget::NO_LATENCY);
    CHECK_EQ(e.message(), NULL);

    CHECK_EQ(t.size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"].size(), 1);
    CHECK_EQ(t["my/pkg/ClassName"]["MethodName"].size(), 2);

    const MethodTarget& target = t["my/pkg/ClassName"]["MethodName"][0];
    CHECK_EQ(target.signature.compare("(Ljava/time/Duration;)V"), 0);
    CHECK_EQ(target.latency, 20);

    const MethodTarget& another_target = t["my/pkg/ClassName"]["MethodName"][1];
    CHECK_EQ(another_target.signature.compare("()V"), 0);
    CHECK_EQ(another_target.latency, 100000000);
}

TEST_CASE(Instrument_test_handleTarget_wrongSignature) {
    Targets t;
    Error e = handleTarget(t, "my.pkg.ClassName.MethodName(Ljava.time.Duration;)V:20ns", MethodTarget::NO_LATENCY);
    CHECK_EQ((bool) e.message(), true);
}

#endif // __linux__
