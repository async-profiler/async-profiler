/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.span;

import one.profiler.Span;

public class SpanApp {

    public static final int IDLE_SPANS = 20;

    private static volatile long sink;

    private static long busy(long millis) {
        long result = 0;
        long deadline = System.currentTimeMillis() + millis;
        while (System.currentTimeMillis() < deadline) {
            for (int i = 0; i < 10000; i++) {
                result += i * 31 + (result >> 1);
            }
        }
        return result;
    }

    public static void main(String[] args) throws Exception {
        // Unconditional spans are always recorded, whether or not they enclose a sample.
        long span = Span.start();
        sink = busy(300);
        Span.end(span, "busyRequest");

        span = Span.start();
        Span.end(span, "idleRequest");

        span = Span.start();
        sink = busy(80);
        Span.end(span, null);  // null tag

        // Optional spans around CPU-busy work: recorded because they enclose samples.
        for (int i = 0; i < 5; i++) {
            span = Span.start();
            sink = busy(100);
            Span.endIfProfiled(span, "busyOptional");
        }

        // Identical idle (off-CPU) workload recorded two ways. Unconditional spans are all
        // recorded; optional ones are mostly skipped, since a sleeping thread takes no samples.
        for (int i = 0; i < IDLE_SPANS; i++) {
            span = Span.start();
            Thread.sleep(5);
            Span.endIfProfiled(span, "idleOptional");
        }
        for (int i = 0; i < IDLE_SPANS; i++) {
            span = Span.start();
            Thread.sleep(5);
            Span.end(span, "idleNormal");
        }
    }
}
