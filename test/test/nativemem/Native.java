/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativemem;

public class Native {
    static {
        System.loadLibrary("jnimalloc");
    }

    public static native long malloc(long size);

    public static native long realloc(long addr, long size);

    public static native long calloc(long num, long size);

    public static native long free(long addr);

    public static native long posixMemalign(long alignment, long size);

    public static native long alignedAlloc(long alignment, long size);
}
