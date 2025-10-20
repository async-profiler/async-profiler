/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativelock;

public class CallsWrLock {

    public static void main(String[] args) throws InterruptedException {
        final boolean once = args.length > 0 && args[0].equals("once");
        
        if (once) {
            NativeLock.createWrLockContention();
        } else {
            while (true) {
                NativeLock.createWrLockContention();
                Thread.sleep(100);
            }
        }
    }
}