/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfrconverter;

final class Tracer {

    static final long TRACE_DURATION_MS = 500;
    private static final long SLEEP_DURATION_MS = 200;

    private static void traceMethod(Runnable work) {
        work.run();
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
        // The latency filter is per-thread, so the work we expect to retain must run
        // on the same thread as the enclosing traceMethod span.
        Thread t1 = new Thread(() -> traceMethod(() -> {
            try {
                showcase1();
                showcase2();
                // Keep the span above the latency threshold so it is retained
                Thread.sleep(TRACE_DURATION_MS - 2 * SLEEP_DURATION_MS);
            } catch (InterruptedException exception) {}
        }), "thread1");
        t1.start();
        t1.join();

        // This span is too short to pass the latency threshold, so its samples
        // (showcase3) should be filtered away
        Thread t2 = new Thread(() -> traceMethod(() -> {
            try {
                showcase3();
            } catch (InterruptedException exception) {}
        }), "thread2");
        t2.start();
        t2.join();
    }
}
