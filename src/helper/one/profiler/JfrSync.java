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

/**
 * Synchronize async-profiler recording with an existing JFR recording.
 */
class JfrSync implements FlightRecorderListener {
    // Keep in sync with EventMask
    private static final int EM_CPU            = 1;
    private static final int EM_ALLOC          = 2;
    private static final int EM_LOCK           = 4;

    // Keep in sync with EVENT_MASK_SIZE in C++
    private static final int EVENT_MASK_SIZE = 6;

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

    public static void stop() {
        Recording recording = masterRecording;
        if (recording != null) {
            // Disable state change notification before stopping
            masterRecording = null;
            recording.stop();
        }
    }

    private static void disableBuiltinEvents(Recording recording, int eventMask) {
        if ((eventMask & EM_CPU) != 0) {
            recording.disable("jdk.ExecutionSample");
            recording.disable("jdk.NativeMethodSample");
        }
        if ((eventMask & EM_ALLOC) != 0) {
            recording.disable("jdk.ObjectAllocationInNewTLAB");
            recording.disable("jdk.ObjectAllocationOutsideTLAB");
            recording.disable("jdk.ObjectAllocationSample");
            recording.disable("jdk.OldObjectSample");
        }
        if ((eventMask & EM_LOCK) != 0) {
            recording.disable("jdk.JavaMonitorEnter");
            recording.disable("jdk.ThreadPark");
        }
        // No built-in event related to EM_WALL
        // No built-in event related to EM_NATIVEMEM
        // No need to disable built-in event related to EM_METHOD_TRACE

        eventMask >>>= EVENT_MASK_SIZE;
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
