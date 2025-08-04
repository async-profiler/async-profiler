/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordingFile;
import one.profiler.test.Assert;
import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.*;
import java.util.stream.Collectors;

public class JfrTests {

    @Test(mainClass = CpuLoad.class, agentArgs = "start,event=cpu,file=%profile.jfr")
    public void cpuLoad(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        String jfrOutPath = p.getFilePath("%profile");
        String spikePattern = "test/jfr/CpuLoad.cpuSpike.*";
        String normalLoadPattern = "test/jfr/CpuLoad.normalCpuLoad.*";

        Output out = Output.convertJfrToCollapsed(jfrOutPath, "--to", "1500");
        assert !out.contains(spikePattern);
        assert out.contains(normalLoadPattern);

        out = Output.convertJfrToCollapsed(jfrOutPath,"--from", "1500", "--to", "3500");
        assert out.contains(spikePattern);
        assert out.contains(normalLoadPattern);

        out = Output.convertJfrToCollapsed(jfrOutPath,"--from", "3500");
        assert !out.contains(spikePattern);
        assert out.contains(normalLoadPattern);
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
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
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
    @Test(mainClass = JfrMultiModeProfiling.class, agentArgs = "start,event=cpu,alloc,lock=0,quiet,jfr,file=%f", output = true)
    public void parseMultiModeRecording(TestProcess p) throws Exception {
        Output output = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;

        List<String> standardOutput = output.stream().collect(Collectors.toList());
        long totalLockDurationMillis = Long.parseLong(standardOutput.get(0));
        int totalNumberOfLocks = Integer.parseInt(standardOutput.get(1));

        double jfrTotalLockDurationMillis = 0;
        Map<String, Integer> eventsCount = new HashMap<>();
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                String eventName = event.getEventType().getName();
                if (eventName.equals("jdk.JavaMonitorEnter")) {
                    jfrTotalLockDurationMillis += event.getDuration().toNanos() / 1_000_000.0;
                }
                eventsCount.put(eventName, eventsCount.getOrDefault(eventName, 0) + 1);
            }
        }

        Assert.isGreater(eventsCount.get("jdk.ExecutionSample"), 50);
        Assert.isGreaterOrEqual(eventsCount.get("jdk.JavaMonitorEnter"), totalNumberOfLocks - 5);
        Assert.isLessOrEqual(eventsCount.get("jdk.JavaMonitorEnter"), totalNumberOfLocks + 5);
        Assert.isGreater(jfrTotalLockDurationMillis / totalLockDurationMillis, 0.80);
        Assert.isGreater(eventsCount.get("jdk.ObjectAllocationInNewTLAB"), 50);
    }

    /**
     * Test to validate profiling output with "--all" flag without event override.
     *
     * @param p The test process to profile with.
     * @throws Exception Any exception thrown during profiling JFR output parsing.
     */
    @Test(mainClass = JfrMultiModeProfiling.class, agentArgs = "start,all,file=%f.jfr", nameSuffix = "noOverride")
    @Test(mainClass = JfrMultiModeProfiling.class, agentArgs = "start,all,alloc=100,file=%f.jfr", nameSuffix = "overrideAlloc")
    public void allModeNoEventOverride(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Set<String> events = new HashSet<>();
        String vmSpecificationVersion = null;
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                String eventName = event.getEventType().getName();

                if (eventName.equals("jdk.InitialSystemProperty") &&
                    event.getString("key").equals("java.vm.specification.version")) {
                    vmSpecificationVersion = event.getString("value");
                }

                events.add(eventName);
            }
        }
        if (p.currentOs() == Os.LINUX) { // macOS uses Wall Clock profiling engine
            assert events.contains("jdk.ExecutionSample"); // cpu profiling
        }
        assert events.contains("jdk.JavaMonitorEnter"); // lock profiling
        assert events.contains("jdk.ObjectAllocationInNewTLAB"); // alloc profiling
        assert events.contains("profiler.WallClockSample"); // wall clock profiling
        assert events.contains("profiler.LiveObject") || checkJdkVersionEarlierThan11(vmSpecificationVersion); // profiling of live objects
        assert events.contains("profiler.Malloc"); // nativemem profiling
        assert events.contains("profiler.Free"); // nativemem profiling
    }

    /**
     * Test to validate profiling output with "--all" flag with event override
     *
     * @param p The test process to profile with.
     * @throws Exception Any exception thrown during profiling JFR output parsing.
     */
    @Test(mainClass = JfrMultiModeProfiling.class, agentArgs = "start,all,event=java.util.Properties.getProperty,alloc=100,file=%f.jfr")
    public void allModeEventOverride(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Set<String> events = new HashSet<>();
        String vmSpecificationVersion = null;
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                String eventName = event.getEventType().getName();

                if (eventName.equals("jdk.InitialSystemProperty") &&
                    event.getString("key").equals("java.vm.specification.version")) {
                    vmSpecificationVersion = event.getString("value");
                }

                events.add(eventName);
                if (eventName.equals("jdk.ExecutionSample")) {
                    // This means that only instrumented method was profiled and overall CPU profiling was skipped
                    assert event.getStackTrace().toString().contains("java.util.Properties.getProperty");
                }
            }
        }
        assert events.contains("jdk.JavaMonitorEnter"); // lock profiling
        assert events.contains("jdk.ObjectAllocationInNewTLAB"); // alloc profiling
        assert events.contains("profiler.WallClockSample"); // wall clock profiling
        assert events.contains("profiler.LiveObject") || checkJdkVersionEarlierThan11(vmSpecificationVersion); // profiling of live objects
        assert events.contains("profiler.Malloc"); // nativemem profiling
        assert events.contains("profiler.Free"); // nativemem profiling
    }

    /**
     * Test to validate time to safepoint profiling
     *
     * @param p The test process to profile with.
     * @throws Exception Any exception thrown during profiling JFR output parsing.
     */
    @Test(mainClass = Ttsp.class, agentArgs = "start,event=cpu,ttsp,interval=1ms,jfr,file=%f")
    public void ttsp(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        assert !containsSamplesOutsideWindow(p) : "Expected no samples outside of ttsp window";

        Output out = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert out.samples("delaySafepoint") >= 10;
    }

    /**
     * Test to validate time to safepoint profiling (recording the windows only, profiling starts immediately)
     *
     * @param p The test process to profile with.
     * @throws Exception Any exception thrown during profiling JFR output parsing.
     */
    @Test(mainClass = Ttsp.class, agentArgs = "start,event=cpu,ttsp,nostop,interval=1ms,jfr,file=%f")
    public void ttspNostop(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        assert containsSamplesOutsideWindow(p) : "Expected to find samples outside of ttsp window";
    }

    private boolean containsSamplesOutsideWindow(TestProcess p) throws Exception {
        TreeMap<Instant, Instant> profilerWindows = new TreeMap<>();
        List<RecordedEvent> samples = new ArrayList<>();
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                if (event.getEventType().getName().equals("profiler.Window")) {
                    profilerWindows.put(event.getStartTime(), event.getEndTime());
                } else if (event.getEventType().getName().equals("jdk.ExecutionSample")) {
                    samples.add(event);
                }
            }
        }

        return samples.stream().anyMatch(event -> {
            Map.Entry<Instant, Instant> entry = profilerWindows.floorEntry(event.getStartTime().plus(10, ChronoUnit.MILLIS));
            Instant entryEnd = entry == null ? Instant.MIN : entry.getValue().plus(10, ChronoUnit.MILLIS);
            // check that the current sample takes place during a profiling window, allowing for a 10ms buffer at each end
            return entryEnd.isBefore(event.getStartTime());
        });
    }

    private static boolean checkJdkVersionEarlierThan11(String vmSpecificationVersion) {
        if (vmSpecificationVersion == null) {
            throw new IllegalArgumentException("vmSpecificationVersion should not be null");
        }
        return vmSpecificationVersion.startsWith("1.") || Integer.parseInt(vmSpecificationVersion.split("\\.")[0]) < 11;
    }
}
