/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativemem;

public class Native {
    static {
        System.loadLibrary("jnimalloc");
    }

    public static native long malloc(int size);

    public static native long realloc(long addr, int size);

    public static native long calloc(long num, int size);

    public static native long free(long addr);
}
