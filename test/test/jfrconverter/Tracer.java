/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfrconverter;

import java.util.concurrent.CountDownLatch;

final class Tracer {

    static final long TRACE_DURATION_MS = 500;
    private static final long SLEEP_DURATION_MS = 200;

    private static void traceMethod(CountDownLatch latch) throws InterruptedException {
        latch.await();
    }

    private static void showcase1() throws InterruptedException {
        Thread.sleep(SLEEP_DURATION_MS);
    }

    private static void showcase2() throws InterruptedException {
        Thread.sleep(SLEEP_DURATION_MS);
    }

    private static void showcase3() throws InterruptedException {
        Thread.sleep(SLEEP_DURATION_MS);
    }

    public static void main(String[] args) throws InterruptedException {
        CountDownLatch latch1 = new CountDownLatch(1);
        long startNanos = System.nanoTime();
        Thread t1 = new Thread(() -> {
            try {
                traceMethod(latch1);
            } catch (InterruptedException exception) {}
        }, "thread1");
        t1.start();

        // While traceMethod is waiting, we can do some stuff and expect to see it
        // as part of the trace
        showcase1();
        showcase2();

        while (System.nanoTime() - startNanos < TRACE_DURATION_MS * 1_000_000) {
            Thread.sleep(50);
        }
        latch1.countDown();
        t1.join();

        CountDownLatch latch2 = new CountDownLatch(1);
        Thread t2 = new Thread(() -> {
            try {
                traceMethod(latch2);
            } catch (InterruptedException exception) {}
        }, "thread2");
        t2.start();

        // This should be filtered away, we won't let the trace last enough
        showcase3();

        latch2.countDown();
        t2.join();
    }
}
