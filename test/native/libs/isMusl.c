/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jni.h>

#ifdef __APPLE__
const static int musl = 0;
#else

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

const static int musl = confstr(_CS_GNU_LIBC_VERSION, NULL, 0) == 0 && errno != 0;

#endif

JNIEXPORT jboolean JNICALL Java_one_profiler_test_Runner_isMusl(JNIEnv* env, jclass cls) {
    return musl;
}

