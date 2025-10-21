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

TEST_CASE(Instrument_test_addTarget_default) {
    Targets t;
    Error e = addTarget(t, "my.pkg.ClassName.MethodName", -28);
    CHECK_EQ(e.message(), NULL);

    long latency;
    CHECK_EQ(findLatency(&t["my/pkg/ClassName"], "MethodName", "", latency), true);
    CHECK_EQ(latency, -28);
}

TEST_CASE(Instrument_test_addTarget_latency) {
    Targets t;
    Error e = addTarget(t, "my.pkg.ClassName.MethodName:20ns", NO_LATENCY);
    CHECK_EQ(e.message(), NULL);

    long latency;
    CHECK_EQ(findLatency(&t["my/pkg/ClassName"], "MethodName", "", latency), true);
    CHECK_EQ(latency, 20);
}

TEST_CASE(Instrument_test_addTarget_signatureAndLatency) {
    Targets t;
    Error e = addTarget(t, "my.pkg.ClassName.MethodName(Ljava/time/Duration;)V:20ns", NO_LATENCY);
    CHECK_EQ(e.message(), NULL);

    long latency;
    CHECK_EQ(findLatency(&t["my/pkg/ClassName"], "MethodName", "(Ljava/time/Duration;)V", latency), true);
    CHECK_EQ(latency, 20);
}

TEST_CASE(Instrument_test_addTarget_manyClasses) {
    Targets t;
    Error e = addTarget(t, "my.pkg.ClassName.MethodName(Ljava/time/Duration;)V:20ns", NO_LATENCY);
    CHECK_EQ(e.message(), NULL);
    e = addTarget(t, "my.pkg.AnotherClass.MethodName()V:100ms", NO_LATENCY);
    CHECK_EQ(e.message(), NULL);

    long latency;
    CHECK_EQ(findLatency(&t["my/pkg/ClassName"], "MethodName", "(Ljava/time/Duration;)V", latency), true);
    CHECK_EQ(latency, 20);
    CHECK_EQ(findLatency(&t["my/pkg/AnotherClass"], "MethodName", "()V", latency), true);
    CHECK_EQ(latency, 100000000);
}

TEST_CASE(Instrument_test_addTarget_manyMethods) {
    Targets t;
    Error e = addTarget(t, "my.pkg.ClassName.MethodName(Ljava/time/Duration;)V:20ns", NO_LATENCY);
    CHECK_EQ(e.message(), NULL);
    e = addTarget(t, "my.pkg.ClassName.AnotherMethod()V:100ms", NO_LATENCY);
    CHECK_EQ(e.message(), NULL);

    long latency;
    CHECK_EQ(findLatency(&t["my/pkg/ClassName"], "MethodName", "(Ljava/time/Duration;)V", latency), true);
    CHECK_EQ(latency, 20);
    CHECK_EQ(findLatency(&t["my/pkg/ClassName"], "AnotherMethod", "()V", latency), true);
    CHECK_EQ(latency, 100000000);
}

TEST_CASE(Instrument_test_addTarget_manySignatures) {
    Targets t;
    Error e = addTarget(t, "my.pkg.ClassName.MethodName(Ljava/time/Duration;)V:20ns", NO_LATENCY);
    CHECK_EQ(e.message(), NULL);
    e = addTarget(t, "my.pkg.ClassName.MethodName()V:100ms", NO_LATENCY);
    CHECK_EQ(e.message(), NULL);

    long latency;
    CHECK_EQ(findLatency(&t["my/pkg/ClassName"], "MethodName", "(Ljava/time/Duration;)V", latency), true);
    CHECK_EQ(latency, 20);
    CHECK_EQ(findLatency(&t["my/pkg/ClassName"], "MethodName", "()V", latency), true);
    CHECK_EQ(latency, 100000000);
}

TEST_CASE(Instrument_test_addTarget_wrongSignature) {
    Targets t;
    Error e = addTarget(t, "my.pkg.ClassName.MethodName(Ljava.time.Duration;)V:20ns", NO_LATENCY);
    CHECK_EQ((bool) e.message(), true);
}

TEST_CASE(Instrument_test_matchesPattern) {
    CHECK_EQ(matchesPattern("someValue", 9, "someValue"), true);
    CHECK_EQ(matchesPattern("someValue", 9, "someValu*"), true);
    CHECK_EQ(matchesPattern("someValue", 9, "s*"), true);
    CHECK_EQ(matchesPattern("someValue", 9, "*"), true);
    CHECK_EQ(matchesPattern("someValue", 9, "someValx*"), false);
    // cpool strings are not zero-terminated
    CHECK_EQ(matchesPattern("someValuexyz", 9, "someValu*"), true);
    CHECK_EQ(matchesPattern("someValuexyz", 9, "someValue"), true);
}

TEST_CASE(Instrument_test_matchesPattern_empty) {
    CHECK_EQ(matchesPattern("", 0, "someValx*"), false);
    CHECK_EQ(matchesPattern("someValue", 9, ""), false);
}

TEST_CASE(Instrument_test_findLatency) {
    MethodTargets t;
    t["meth*"] = 0;
    t["method1"] = 10;
    t["method2(Ljava/time/Duration;)V"] = 11;
    t["method3*"] = 12;
    t["method4(L*"] = 13;

    long latency;
    CHECK_EQ(findLatency(&t, "method1", "()V", latency), true);
    CHECK_EQ(latency, 10);

    CHECK_EQ(findLatency(&t, "method2", "(Ljava/time/Duration;)V", latency), true);
    CHECK_EQ(latency, 11);

    CHECK_EQ(findLatency(&t, "method2", "()V", latency), true);
    CHECK_EQ(latency, 0);

    CHECK_EQ(findLatency(&t, "method3", "(Ljava/time/Duration;)V", latency), true);
    CHECK_EQ(latency, 12);

    CHECK_EQ(findLatency(&t, "method3", "()V", latency), true);
    CHECK_EQ(latency, 12);

    CHECK_EQ(findLatency(&t, "method4", "(Ljava/time/Duration;)V", latency), true);
    CHECK_EQ(latency, 13);

    CHECK_EQ(findLatency(&t, "method4", "()V", latency), true);
    CHECK_EQ(latency, 0);

    CHECK_EQ(findLatency(&t, "methodd1", "(Ljava/time/Duration;)V", latency), true);
    CHECK_EQ(latency, 0);

    CHECK_EQ(findLatency(&t, "nethod1", "(Ljava/time/Duration;)V", latency), false);
}

#endif // __linux__
