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

    public static native void recordSample();
}
