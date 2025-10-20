/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativelock;

public class CallsRdLock {

    public static void main(String[] args) throws InterruptedException {
        final boolean once = args.length > 0 && args[0].equals("once");
        
        if (once) {
            NativeLock.createRdLockContention();
        } else {
            while (true) {
                NativeLock.createRdLockContention();
                Thread.sleep(100);
            }
        }
    }
}