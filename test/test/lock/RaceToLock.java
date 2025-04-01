/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.locks.LockSupport;

public class RaceToLock {

    // Every SHARED_LOCK_INTERVAL iterations of the loop in runSemiShared, we try to lock a shared lock instead of an uncontended lock
    private static final long SHARED_LOCK_INTERVAL = 10;
    private static final long DURATION_MS = 4000;

    private static final Lock sharedLock = new ReentrantLock(true);

    private static volatile boolean exitRequested = false;

    private static void doWork() {
        LockSupport.parkNanos(1);
    }

    private static void runShared() {
        while (!exitRequested) {
            sharedLock.lock();
            try {
                doWork();
            } finally {
                sharedLock.unlock();
            }
        }
    }

    private static void runSemiShared() {
        long count = 0;
        Lock nonsharedLock = new ReentrantLock(true);
        while (!exitRequested) {
            Lock lock = count % SHARED_LOCK_INTERVAL == 0 ? sharedLock : nonsharedLock;

            lock.lock();
            try {
                doWork();
            } finally {
                lock.unlock();
            }

            ++count;
        }
    }

    public static void main(String[] args) throws InterruptedException {
        Thread[] threads = {
                new Thread(null, RaceToLock::runShared, "shared1"),
                new Thread(null, RaceToLock::runShared, "shared2"),

                new Thread(null, RaceToLock::runSemiShared, "semiShared1"),
                new Thread(null, RaceToLock::runSemiShared, "semiShared2")
        };
        for (Thread t : threads) {
            t.start();
        }

        Thread.sleep(DURATION_MS);
        exitRequested = true;
        for (Thread t : threads) {
            t.join();
        }
    }
}
