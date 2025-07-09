/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import java.util.concurrent.BrokenBarrierException;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.locks.ReentrantLock;

public class SimpleLockTest {

    private static final ReentrantLock lock = new ReentrantLock();
    private static final CyclicBarrier barrier = new CyclicBarrier(10);

    private static synchronized void syncLock() throws InterruptedException {
        Thread.sleep(100);
    }

    private static void semLock() throws InterruptedException {
        lock.lock();
        Thread.sleep(100);
        lock.unlock();
    }

    private static void entryMethod(String test) throws InterruptedException, BrokenBarrierException {
        barrier.await();
        if (test.equals("sync")) {
            syncLock();
        } else if (test.equals("sem")) {
            semLock();
        }
    }

    public static void main(String[] args) throws InterruptedException {
        if (args.length < 1) {
            System.out.println("Please input lock type: sync or sem");
            System.exit(1);
        }

        Runnable runnable = () -> {
            try {
                entryMethod(args[0]);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        };

        Thread[] threads = new Thread[10];
        for (int i = 0; i < threads.length; i++) {
            threads[i] = new Thread(runnable);
            threads[i].start();
        }

        for (Thread thread : threads) {
            thread.join();
        }
    }
}
