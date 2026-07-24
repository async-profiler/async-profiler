/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

import jdk.jfr.Configuration;
import jdk.jfr.FlightRecorder;
import jdk.jfr.FlightRecorderListener;
import jdk.jfr.Recording;
import jdk.jfr.RecordingState;

import java.io.IOException;
import java.nio.file.NoSuchFileException;
import java.nio.file.Paths;
import java.text.ParseException;
import java.util.StringTokenizer;
import java.util.concurrent.locks.LockSupport;

/**
 * Synchronize async-profiler recording with an existing JFR recording.
 */
class JfrSync implements FlightRecorderListener {
    // Keep in sync with EventCategory
    private static final int EC_CPU        = 0;
    private static final int EC_ALLOC      = 1;
    private static final int EC_LOCK       = 2;
    private static final int EC_CATEGORIES = 8;

    // Keep in sync with JfrOption
    private static final int NO_SYSTEM_INFO  = 1;
    private static final int NO_SYSTEM_PROPS = 2;
    private static final int NO_NATIVE_LIBS  = 4;
    private static final int NO_CPU_LOAD     = 8;
    private static final int NO_HEAP_SUMMARY = 16;

    private static volatile Recording masterRecording;

    private JfrSync() {
    }

    static {
        FlightRecorder.addListener(new JfrSync());
    }

    @Override
    public void recordingStateChanged(Recording recording) {
        if (recording == masterRecording && recording.getState() == RecordingState.STOPPED) {
            masterRecording = null;
            stopProfiler();
        }
    }

    public static void start(String fileName, String settings, int eventMask) throws IOException, ParseException {
        Recording recording;
        if (settings.startsWith("+")) {
            recording = new Recording();
            for (StringTokenizer st = new StringTokenizer(settings, "+"); st.hasMoreTokens(); ) {
                recording.enable(st.nextToken());
            }
        } else {
            try {
                recording = new Recording(Configuration.getConfiguration(settings));
            } catch (NoSuchFileException e) {
                recording = new Recording(Configuration.create(Paths.get(settings)));
            }
            disableBuiltinEvents(recording, eventMask);
        }

        masterRecording = recording;

        recording.setDestination(Paths.get(fileName));
        recording.setToDisk(true);
        recording.setDumpOnExit(true);
        recording.start();
    }

    public static boolean stop() {
        Recording recording = masterRecording;
        if (recording != null) {
            // Disable state change notification before stopping
            masterRecording = null;
            try {
                recording.stop();
            } catch (IllegalStateException e) {
                // Workaround the JDK issue: JFR shutdown hook may stop the recording concurrently
                // then populate the target file outside the state lock.
                // Once the file is completely written, the recording state is changed to CLOSED.
                for (int pause = 10; recording.getState() != RecordingState.CLOSED && pause < 1000; pause *= 2) {
                    LockSupport.parkNanos(pause * 1_000_000L);
                }
                return recording.getState() == RecordingState.CLOSED;
            }
        }
        return true;
    }

    private static void disableBuiltinEvents(Recording recording, int eventMask) {
        if ((eventMask & (1 << EC_CPU)) != 0) {
            recording.disable("jdk.ExecutionSample");
            recording.disable("jdk.NativeMethodSample");
        }
        if ((eventMask & (1 << EC_ALLOC)) != 0) {
            recording.disable("jdk.ObjectAllocationInNewTLAB");
            recording.disable("jdk.ObjectAllocationOutsideTLAB");
            recording.disable("jdk.ObjectAllocationSample");
            recording.disable("jdk.OldObjectSample");
        }
        if ((eventMask & (1 << EC_LOCK)) != 0) {
            recording.disable("jdk.JavaMonitorEnter");
            recording.disable("jdk.ThreadPark");
        }
        // No need to disable jdk.MethodTrace, and no built-in events correspond to other event categories

        eventMask >>>= EC_CATEGORIES;
        // Shifted JfrOption values
        if ((eventMask & NO_SYSTEM_INFO) != 0) {
            recording.disable("jdk.OSInformation");
            recording.disable("jdk.CPUInformation");
            recording.disable("jdk.JVMInformation");
        }
        if ((eventMask & NO_SYSTEM_PROPS) != 0) {
            recording.disable("jdk.InitialSystemProperty");
        }
        if ((eventMask & NO_NATIVE_LIBS) != 0) {
            recording.disable("jdk.NativeLibrary");
        }
        if ((eventMask & NO_CPU_LOAD) != 0) {
            recording.disable("jdk.CPULoad");
        }
        if ((eventMask & NO_HEAP_SUMMARY) != 0) {
            recording.disable("jdk.GCHeapSummary");
        }
    }

    private static native void stopProfiler();

    // JNI helper
    static Integer box(int n) {
        return n;
    }
}
