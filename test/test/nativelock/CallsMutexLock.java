/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativelock;

public class CallsMutexLock {

    public static void main(String[] args) throws InterruptedException {
        final boolean once = args.length > 0 && args[0].equals("once");
        
        if (once) {
            NativeLock.createMutexContention();
        } else {
            while (true) {
                NativeLock.createMutexContention();
                Thread.sleep(100);
            }
        }
    }
}