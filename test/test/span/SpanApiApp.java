/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.span;

import one.profiler.AsyncProfiler;
import one.profiler.Recording;
import one.profiler.Span;

/**
 * Loads async-profiler through its Java API and records spans across the session lifecycle.
 */
public class SpanApiApp {

    private static volatile long sink;

    private static void busy(long millis) {
        long result = 0;
        long deadline = System.currentTimeMillis() + millis;
        while (System.currentTimeMillis() < deadline) {
            for (int i = 0; i < 10000; i++) {
                result += i * 31 + (result >> 1);
            }
        }
        sink = result;
    }

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();

        // No session has ever started: spans are dropped.
        Span.end(Span.start(), "beforeSession");

        profiler.execute("start,event=cpu,interval=1ms,file=" + args[0]);
        assert Recording.state() == Recording.RUNNING;

        long span = Span.start();
        busy(300);
        Span.end(span, "duringSession");

        profiler.execute("stop");
        assert Recording.state() == Recording.STOPPED;

        // Session stopped: spans are dropped again.
        Span.end(Span.start(), "afterSession");
    }
}
