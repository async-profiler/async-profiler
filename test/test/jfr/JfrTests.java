/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordingFile;
import one.profiler.test.Assert;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

public class JfrTests {

    @Test(mainClass = RegularPeak.class)
    public void regularPeak(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 6 -f %f.jfr");
        String jfrOutPath = p.getFile("%f").getAbsolutePath();
        out = Output.convertJfrToCollapsed(jfrOutPath, "--to", "2500");
        assert !out.contains("test/jfr/Cache\\.lambda\\$calculateTop\\$1");
        out = Output.convertJfrToCollapsed(jfrOutPath,"--from", "2500", "--to", "5000");
        assert out.samples("test/jfr/Cache\\.lambda\\$calculateTop\\$1") >= 1;
        out = Output.convertJfrToCollapsed(jfrOutPath,"--from", "5000");
        assert !out.contains("test/jfr/Cache\\.lambda\\$calculateTop\\$1");
    }

    /**
     * Test to validate JDK APIs to parse Cpu profiling JFR output
     *
     * @param p The test process to profile with.
     * @throws Exception Any exception thrown during profiling JFR output parsing.
     */
    @Test(mainClass = JfrCpuProfiling.class)
    public void parseRecording(TestProcess p) throws Exception {
        p.profile("-d 3 -e cpu -f %f.jfr");
        StringBuilder builder = new StringBuilder();
        try (RecordingFile recordingFile = new RecordingFile(Paths.get(p.getFile("%f").getAbsolutePath()))) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                builder.append(event);
            }
        }

        String parsedOut = builder.toString();
        assert parsedOut.contains("jdk.ExecutionSample");
        assert parsedOut.contains("test.jfr.JfrCpuProfiling.method1()");
    }

    /**
     * Test to validate JDK APIs to parse Multimode profiling JFR output
     *
     * @param p The test process to profile with.
     * @throws Exception Any exception thrown during profiling JFR output parsing.
     */
    @Test(mainClass = JfrMutliModeProfiling.class, agentArgs = "start,event=cpu,alloc,lock,jfr,file=%f")
    public void parseMultiModeRecording(TestProcess p) throws Exception {
        p.waitForExit();
        Map<String, Integer> eventsCount = new HashMap<>();
        try (RecordingFile recordingFile = new RecordingFile(Paths.get(p.getFile("%f").getAbsolutePath()))) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                String eventName = event.getEventType().getName();
                eventsCount.put(eventName, eventsCount.getOrDefault(eventName, 0) + 1);
            }
        }

        Assert.isGreater(eventsCount.get("jdk.ExecutionSample"), 50);
        Assert.isGreater(eventsCount.get("jdk.JavaMonitorEnter"), 50);
        Assert.isGreater(eventsCount.get("jdk.ObjectAllocationInNewTLAB"), 50);
    }
}
