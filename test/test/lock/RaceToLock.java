/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.locks.LockSupport;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class RaceToLock {

    private final Lock sharedLock = new ReentrantLock(true);
    private final Object sharedObj = new Object();
    private long sharedWaitTime = 0;

    private final Random random = new Random();
    private final Lock lock1 = new ReentrantLock(true);
    private final Lock lock2 = new ReentrantLock(true);
    private volatile long randomWaitTime;

    private volatile boolean exitRequested;

    private void doWork() {
        LockSupport.parkNanos(1);
    }

    private void runSharedCounter() {
        while (!exitRequested) {
            long start = System.nanoTime();
            sharedLock.lock();
            sharedWaitTime += System.nanoTime() - start;

            try {
                doWork();
            } finally {
                sharedLock.unlock();
            }
        }
    }

    private void runRandomCounter() {
        while (!exitRequested) {
            Lock lock = random.nextBoolean() ? lock1 : lock2;

            long start = System.nanoTime();
            lock.lock();
            randomWaitTime += System.nanoTime() - start;
            try {
                doWork();
            } finally {
                lock.unlock();
            }
        }
    }

    public void dump() {
        // Referred to from tests.
        System.out.println("sharedWaitTime: " + sharedWaitTime);
        System.out.println("randomWaitTime: " + randomWaitTime);
    }

    public static void main(String[] args) throws InterruptedException {
        RaceToLock app = new RaceToLock();
        Thread[] threads = {
                new Thread(null, app::runSharedCounter, "shared"),
                new Thread(null, app::runSharedCounter, "shared"),

                new Thread(null, app::runRandomCounter, "random"),
                new Thread(null, app::runRandomCounter, "random")
        };
        for (Thread t : threads) {
            t.start();
        }

        Thread.sleep(1000);
        app.exitRequested = true;
        for (Thread t : threads) {
            t.join();
        }

        app.dump();
    }
}
