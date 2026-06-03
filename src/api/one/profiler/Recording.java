/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.MutableCallSite;
import java.nio.ByteBuffer;

/**
 * Tracks async-profiler's recording state and provides timestamps aligned with the profiler clock.
 */
public class Recording {
    private static final MutableCallSite TIMESTAMP_CS = new MutableCallSite(getTimestampMH(null));
    private static final MethodHandle TIMESTAMP_INVOKER = TIMESTAMP_CS.dynamicInvoker();

    /** async-profiler is not loaded. */
    public static final int UNAVAILABLE = 0;
    /** async-profiler is loaded, but no JFR session is running. */
    public static final int STOPPED = 1;
    /** A JFR session is running. */
    public static final int RUNNING = 2;

    static volatile int state;

    static {
        try {
            state = registerNatives();
        } catch (UnsatisfiedLinkError e) {
            state = UNAVAILABLE;
        }
    }

    private Recording() {
    }

    /**
     * @return the current recording state: {@link #UNAVAILABLE}, {@link #STOPPED} or {@link #RUNNING}
     */
    public static int state() {
        return state;
    }

    /**
     * @return the current time in the same clock async-profiler uses for its events,
     *         so span and sample timestamps are comparable
     */
    public static long timestamp() {
        try {
            return (long) TIMESTAMP_INVOKER.invokeExact();
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

    private static MethodHandle getTimestampMH(MethodHandles.Lookup privateLookup) {
        if (privateLookup != null) {
            try {
                Class<?> jvmClass = Class.forName("jdk.jfr.internal.JVM");
                return privateLookup.findStatic(jvmClass, "counterTime", MethodType.methodType(long.class));
            } catch (Exception e) {
                // Fallback to System.nanoTime
            }
        }
        try {
            return MethodHandles.publicLookup().findStatic(System.class, "nanoTime", MethodType.methodType(long.class));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    // Called from JNI to replace the MethodHandle for timestamps
    private static void updateClock(MethodHandles.Lookup privateLookup) {
        TIMESTAMP_CS.setTarget(getTimestampMH(privateLookup));
        MutableCallSite.syncAll(new MutableCallSite[]{TIMESTAMP_CS});
    }

    private static native int registerNatives();

    static native ByteBuffer getThreadLocalBuffer();

    static native void emitSpan(long startTime, long endTime, String tag);
}
