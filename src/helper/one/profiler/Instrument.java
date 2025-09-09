/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

/**
 * Instrumentation helper for Java method profiling.
 */
public class Instrument {

    private Instrument() {
    }

    public static native void recordEntry();

    public static void recordExit(long startTimeNanos, long minLatency) {
        long durationNs = System.nanoTime() - startTimeNanos;
        if (durationNs >= minLatency) {
            recordExit0(durationNs);
        }
    }

    public static native void recordExit0(long durationNs);
}
