/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <jni.h>
#include <math.h>

JNIEXPORT double useCpu() {
    int i = 0;
    double result = 0;

    while (i < 100000000) {
        i++;
        result += sqrt(i);
        result += pow(i, sqrt(i));
    }
    return result;
}

JNIEXPORT double doRecursiveWork(int count) {
    if (count == 0) {
        return useCpu();
    } else {
        return doRecursiveWork(count - 1);
    }
}

JNIEXPORT jdouble JNICALL Java_test_stackwalker_StackGenForUnwindViaDebugFrame_startWork(JNIEnv* env, jclass cls) {
    return doRecursiveWork(3);
}