/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.span;

import one.profiler.Span;

/**
 * Uses the Span API in a loop without loading async-profiler itself. The test attaches the
 * profiler while this runs, exercising the path where spans are used before async-profiler.
 */
public class SpanAttachApp {

    private static volatile long sink;

    public static void main(String[] args) {
        long deadline = System.currentTimeMillis() + 10000;
        while (System.currentTimeMillis() < deadline) {
            long span = Span.start();
            long result = 0;
            long until = System.currentTimeMillis() + 50;
            while (System.currentTimeMillis() < until) {
                for (int i = 0; i < 10000; i++) {
                    result += i * 31 + (result >> 1);
                }
            }
            sink = result;
            Span.end(span, "attachRequest");
        }
    }
}
