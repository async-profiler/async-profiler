/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jni.h>
#include <math.h>

__attribute__((noinline)) double level3(int iterations) {
    double result = 0;
    for (int i = 1; i <= iterations; i++) {
        result += sqrt((double)i) + pow((double)i, 0.5);
    }
    return result;
}

__attribute__((noinline)) double level2(int iterations) {
    return level3(iterations) - 2.0;
}

__attribute__((noinline)) double level1(int iterations) {
    return level2(iterations) * 3.0;
}

JNIEXPORT jdouble JNICALL Java_test_stackwalker_DebugFrameApp_run(JNIEnv* env, jclass cls, jint iterations) {
    return level1(iterations);
}
