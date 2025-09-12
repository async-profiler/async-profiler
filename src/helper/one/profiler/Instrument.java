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

    public static void recordExit(long startTimeNs, long minLatency) {
        long durationNs = System.nanoTime() - startTimeNs;
        if (durationNs >= minLatency) {
            recordExit0(startTimeNs);
        }
    }

    public static native void recordExit0(long startTimeNs);
}
