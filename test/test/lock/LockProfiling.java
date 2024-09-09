/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.locks.LockSupport;
import java.util.concurrent.ConcurrentHashMap;
import java.util.Map;

public class LockProfiling {
    static final int timeOutsideLock = 1_000_000; // 1 ms
    static final ThreadLocal<Double> totalUsefulWork = ThreadLocal.withInitial(() -> 0.0);
    static final ThreadLocal<Double> totalWait = ThreadLocal.withInitial(() -> 0.0);
    static volatile boolean exitRequested = false;

    static final Map<String, Double> totalUsefulWorksMs = new ConcurrentHashMap<>();
    static final Map<String, Double> totalWaitsMs = new ConcurrentHashMap<>();

    public static void main(String[] args) throws InterruptedException {
        Thread[] c1 = contend(0.05);
        Thread[] c2 = contend(0.5);

        Thread.sleep(2 * 1000);
        exitRequested = true;

        for (Thread t : c1) {
            t.join();
        }
        for (Thread t : c2) {
            t.join();
        }
    }

    static Thread[] contend(double fraction) {
        Object lock = new Object();

        long timeUnderLockNs = (long) ((fraction * timeOutsideLock) / (1 - fraction));

        Thread t1 = new Thread(() -> contend(timeUnderLockNs, timeOutsideLock, lock),
                "contend-" + fraction + "-t1");
        t1.start();

        Thread t2 = new Thread(() -> contend(timeUnderLockNs, timeOutsideLock, lock),
                "contend-" + fraction + "-t2");
        t2.start();

        return new Thread[] { t1, t2 };
    }

    static void usefulWork(long ns) {
        LockSupport.parkNanos(ns);
        totalUsefulWork.set(totalUsefulWork.get() + ns);
    }

    static void contend(long timeUnderLockNs, long timeOutsideLock, Object lock) {
        while (!exitRequested) {
            long start = System.nanoTime();
            long delay = ThreadLocalRandom.current().nextLong(0, timeOutsideLock);

            usefulWork(delay);
            synchronized (lock) {
                usefulWork(timeUnderLockNs);
            }
            usefulWork(timeOutsideLock - delay);

            long duration = System.nanoTime() - start;
            long wait = duration - timeUnderLockNs - timeOutsideLock;
            totalWait.set(totalWait.get() + wait);
        }

        String name = Thread.currentThread().getName();
        totalUsefulWorksMs.put(name, totalUsefulWork.get() / 1e6);
        totalWaitsMs.put(name, totalWait.get() / 1e6);
    }

    public static double parseNameForFraction(String name) {
        String prefix = "contend-";
        int start = name.indexOf(prefix) + prefix.length();
        int end = name.indexOf("-t");

        return Double.parseDouble(name.substring(start, end));
    }
}
