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
        if ((eventMask & 1) != 0) {
            recording.disable("jdk.ExecutionSample");
            recording.disable("jdk.NativeMethodSample");
        }
        if ((eventMask & 2) != 0) {
            recording.disable("jdk.ObjectAllocationInNewTLAB");
            recording.disable("jdk.ObjectAllocationOutsideTLAB");
            recording.disable("jdk.ObjectAllocationSample");
        }
        if ((eventMask & 4) != 0) {
            recording.disable("jdk.JavaMonitorEnter");
            recording.disable("jdk.ThreadPark");
        }

        // Shifted JfrOption values
        if ((eventMask & 0x10) != 0) {
            recording.disable("jdk.OSInformation");
            recording.disable("jdk.CPUInformation");
            recording.disable("jdk.JVMInformation");
        }
        if ((eventMask & 0x20) != 0) {
            recording.disable("jdk.InitialSystemProperty");
        }
        if ((eventMask & 0x40) != 0) {
            recording.disable("jdk.NativeLibrary");
        }
        if ((eventMask & 0x80) != 0) {
            recording.disable("jdk.CPULoad");
        }
        if ((eventMask & 0x100) != 0) {
            recording.disable("jdk.GCHeapSummary");
        }
    }

    private static native void stopProfiler();

    // JNI helper
    static Integer box(int n) {
        return n;
    }
}
