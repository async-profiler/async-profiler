/*
 * Copyright 2021 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package one.profiler;

import jdk.jfr.FlightRecorder;
import jdk.jfr.FlightRecorderListener;
import jdk.jfr.Recording;
import jdk.jfr.RecordingState;

import java.nio.file.Path;

/**
 * Appends async-profiler recording to an existing JFR recording.
 */
class JfrCombiner implements FlightRecorderListener {

    private JfrCombiner() {
    }

    @Override
    public void recordingStateChanged(Recording recording) {
        if (recording.getState() == RecordingState.STOPPED) {
            Path path = recording.getDestination();
            if (path != null) {
                appendRecording(path.toString());
            }
        } else if (recording.getState() != RecordingState.CLOSED) {
            disableBuiltinEvents(recording);
        }
    }

    private static void disableBuiltinEvents(Recording recording) {
        recording.disable("jdk.ExecutionSample");
        recording.disable("jdk.NativeMethodSample");
    }

    private static native void appendRecording(String path);

    static {
        if (FlightRecorder.isInitialized()) {
            for (Recording recording : FlightRecorder.getFlightRecorder().getRecordings()) {
                disableBuiltinEvents(recording);
            }
        }

        FlightRecorder.addListener(new JfrCombiner());
    }
}
