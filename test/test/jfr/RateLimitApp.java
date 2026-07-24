/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import one.profiler.Span;

/**
 * Generates a flood of JFR events of three different categories.
 */
public class RateLimitApp {

    private static final long DURATION_MS = 6000;

    private static volatile Object sink;
    private static volatile long counter;

    private static void burnCpu(long deadline) {
        long result = 0;
        while (System.currentTimeMillis() < deadline) {
            for (int i = 0; i < 10_000; i++) {
                result += i * 31 + (result >> 1);
            }
        }
        counter = result;
    }

    private static void allocate(long deadline) {
        while (System.currentTimeMillis() < deadline) {
            sink = new byte[4096];
        }
    }

    private static void emitSpans(long deadline) {
        while (System.currentTimeMillis() < deadline) {
            long span = Span.start();
            counter++;
            Span.end(span, "flood");
        }
    }

    public static void main(String[] args) throws Exception {
        long deadline = System.currentTimeMillis() + DURATION_MS;

        Thread[] threads = {
            new Thread(() -> burnCpu(deadline)),
            new Thread(() -> burnCpu(deadline)),
            new Thread(() -> allocate(deadline)),
            new Thread(() -> emitSpans(deadline)),
        };
        for (Thread thread : threads) {
            thread.start();
        }
        for (Thread thread : threads) {
            thread.join();
        }
    }
}
