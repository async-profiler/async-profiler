/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativelock;

import java.util.ArrayList;
import java.util.List;

public class AllNativeLocks {

    public static void main(String[] args) throws InterruptedException {
        final boolean once = args.length > 0 && args[0].equals("once");
        
        NativeLock.class.getName();
        Thread.sleep(500);
        
        if (once) {
            runAllLockTypesOnce();
        } else {
            while (true) {
                runAllLockTypesOnce();
                Thread.sleep(100);
            }
        }
    }
    
    private static void runAllLockTypesOnce() throws InterruptedException {
        List<Thread> threads = new ArrayList<>();

        for (int i = 0; i < 4; i++) {
            Thread t = new Thread(() -> NativeLock.mutexContentionThread());
            threads.add(t);
            t.start();
        }
         
        for (int i = 0; i < 6; i++) {
            Thread t = new Thread(() -> NativeLock.rdlockContentionThread());
            threads.add(t);
            t.start();
        }
        
        for (int i = 0; i < 2; i++) {
            Thread t = new Thread(() -> NativeLock.wrlockContentionThread());
            threads.add(t);
            t.start();
        }
        
        for (Thread t : threads) {
            t.join();
        }
    }
}