/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jni.h>
#include <math.h>

double doCpuTask() {
    int i = 0;
    double result = 0;

    while (i < 10000000) {
        i++;
        result += sqrt(i);
        result += pow(i, sqrt(i));
    }
    return result;
}

__attribute__((unused)) double generateDeepStack(int count) {
    char frame[0x20000];
    (void)frame;

    if (count == 0) {
        return doCpuTask();
    } else {
        return generateDeepStack(count - 1);
    }
}

__attribute__((unused)) JNIEXPORT void JNICALL Java_test_stackwalker_Stackwalker_walkStackLargeFrame(JNIEnv* env, jclass cls) {
    char frame[0x50000];
    (void)frame;
    (void)doCpuTask();
}

__attribute__((unused)) JNIEXPORT void JNICALL Java_test_stackwalker_Stackwalker_walkStackDeepStack(JNIEnv* env, jclass cls) {
    char frame[0x30000];
    (void)frame;
    (void)generateDeepStack(6);
}

JNIEXPORT void JNICALL Java_test_stackwalker_Stackwalker_walkStackComplete(JNIEnv* env, jclass cls) {
    (void)doCpuTask();
}
