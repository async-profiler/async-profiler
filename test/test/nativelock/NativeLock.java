/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativelock;

public class NativeLock {
    static {
        System.loadLibrary("jninativelocks");
    }

    public static native void createMutexContention();
    public static native void createRdLockContention();
    public static native void createWrLockContention();
}
