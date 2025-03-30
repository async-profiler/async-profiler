/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.locks.LockSupport;
import java.util.concurrent.ThreadLocalRandom;

public class RaceToLock {

    private final Lock sharedLock = new ReentrantLock(true);
    private final Lock[] locks = {
            new ReentrantLock(true),
            new ReentrantLock(true),
            new ReentrantLock(true)
    };
    private volatile boolean exitRequested;

    private static void doWork() {
        LockSupport.parkNanos(1);
    }

    private void runSharedCounter() {
        while (!exitRequested) {
            try {
                sharedLock.lock();
                doWork();
            } finally {
                sharedLock.unlock();
            }
        }
    }

    private void runRandomCounter() {
        ThreadLocalRandom random = ThreadLocalRandom.current();
        while (!exitRequested) {
            Lock lock = locks[random.nextInt(locks.length)];
            try {
                lock.lock();
                doWork();
            } finally {
                lock.unlock();
            }
        }
    }

    public static void main(String[] args) throws InterruptedException {
        RaceToLock app = new RaceToLock();
        Thread[] threads = {
                new Thread(null, app::runSharedCounter, "shared1"),
                new Thread(null, app::runSharedCounter, "shared2"),

                new Thread(null, app::runRandomCounter, "random1"),
                new Thread(null, app::runRandomCounter, "random2")
        };
        for (Thread t : threads) {
            t.start();
        }

        Thread.sleep(2000);
        app.exitRequested = true;
        for (Thread t : threads) {
            t.join();
        }
    }
}
