/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

/**
 * Helper class to call JNI RegisterNatives in a trusted context.
 */
class LockTracer {

    private LockTracer() {
    }

    // Workaround for JDK-8238460: we need to construct at least two frames
    // belonging to the bootstrap class loader for RegisterNatives not to emit a warning.
    static void setEntry(long entry) {
        setEntry0(entry);
    }

    private static native void setEntry0(long entry);
}
