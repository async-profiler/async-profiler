/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativelock;

public class CallsWrLock {

    public static void main(String[] args) throws InterruptedException {
       NativeLock.class.getName();
        Thread.sleep(500);
        
        NativeLock.createWrLockContention();
    }
}
