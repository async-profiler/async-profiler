/*
 * Copyright the async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

import one.jfr.JfrReader;
import one.jfr.event.ProcessSample;
import one.profiler.test.*;

import java.io.IOException;
import java.util.List;
import java.util.stream.Collectors;
import java.util.Comparator;

public class ProcTests {

    @Test(mainClass = BasicApp.class, os = Os.LINUX)
    public void basicProcessSampling(TestProcess p) throws Exception {
        p.profile("--proc 1 -d 3 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);
            assert !events.isEmpty();

            List<ProcessSample> appSamples = events.stream()
                    .filter(e -> e.cmdLine != null && e.cmdLine.contains("BasicApp"))
                    .collect(Collectors.toList());

            Assert.isEqual(appSamples.size(), 2); // We discard the first sample
        }
    }

// TODO(issue-1432): Re-enable after tiered integration tests are supported.
//     @Test(mainClass = BasicApp.class, os = Os.LINUX)
    public void processSamplingWithZeroSamplingPeriod(TestProcess p) throws Exception {
        p.profile("--proc 0 -d 2 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);

            assert events.isEmpty();
        }
    }

//     @Test(mainClass = BasicApp.class, os = Os.LINUX)
    public void processEvenSamplingInterval(TestProcess p) throws Exception {
        long startTime = System.currentTimeMillis();
        p.profile("--proc 2 -d 8 -f %f.jfr");

        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);

            List<ProcessSample> appSamples = events.stream()
                    .filter(e -> e.cmdLine != null && e.cmdLine.contains("BasicApp"))
                    .collect(Collectors.toList());

            Assert.isEqual(appSamples.size(), 3);
        }
    }

//     @Test(mainClass = BasicApp.class, os = Os.LINUX)
    public void processSamplingWithAllMode(TestProcess p) throws Exception {
        p.profile("--all -d 60 -f %f.jfr", false, 61);
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> procEvents = jfr.readAllEvents(ProcessSample.class);
            assert !procEvents.isEmpty();
        }
    }

//     @Test(mainClass = BasicApp.class, os = Os.LINUX)
    public void validateProcessFields(TestProcess p) throws Exception {
        p.profile("--proc 1 -d 5 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);
            assert !events.isEmpty();

            ProcessSample sample = events.stream()
                    .filter(e -> e.cmdLine != null && e.cmdLine.contains("BasicApp"))
                    .findAny()
                    .orElse(null);

            assert sample != null;

            Assert.isGreater(sample.pid, 0);
            Assert.isGreaterOrEqual(sample.ppid, 0);
            assert sample.name != null && !sample.name.isEmpty();
            assert sample.cmdLine != null;
            Assert.isGreaterOrEqual(sample.uid, 0);
            Assert.isNotEqual(sample.state, 0);
            Assert.isGreater(sample.processStartTime, 0);
            Assert.isGreater(sample.cpuUser, 0);
            Assert.isGreater(sample.cpuSystem, 0);
            Assert.isGreater(sample.cpuPercent, 0);
            Assert.isGreater(sample.threads, 0);
            Assert.isGreaterOrEqual(sample.vmSize, 0);
            Assert.isGreaterOrEqual(sample.vmRss, 0);
            Assert.isGreaterOrEqual(sample.rssAnon, 0);
            Assert.isGreaterOrEqual(sample.rssFiles, 0);
            Assert.isGreaterOrEqual(sample.rssShmem, 0);
            Assert.isGreaterOrEqual(sample.minorFaults, 0);
            Assert.isGreaterOrEqual(sample.majorFaults, 0);
            Assert.isGreaterOrEqual(sample.ioRead, 0);
            Assert.isGreaterOrEqual(sample.ioWrite, 0);
        }
    }

//     @Test(mainClass = IoIntensiveApp.class, os = Os.LINUX)
    public void validateIoStats(TestProcess p) throws Exception {
        p.profile("--proc 1 -d 8 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);
            assert !events.isEmpty();

            ProcessSample sample = events.stream()
                    .filter(e -> e.cmdLine != null && e.cmdLine.contains("IoIntensiveApp"))
                    .max(Comparator.comparingLong(e -> e.time))
                    .orElse(null);

            assert sample != null;

            Assert.isGreaterOrEqual(sample.ioRead, 0);
            Assert.isGreaterOrEqual(sample.ioWrite, 64 * 1024);
        }
    }

//     @Test(mainClass = MultiThreadApp.class, os = Os.LINUX)
    public void validateThreadCounts(TestProcess p) throws Exception {
        p.profile("--proc 1 -d 6 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);

            ProcessSample multiThreadSample = events.stream()
                    .filter(e -> e.cmdLine != null && e.cmdLine.contains("MultiThreadApp"))
                    .findFirst()
                    .orElse(null);

            assert multiThreadSample != null;
            Assert.isGreaterOrEqual(multiThreadSample.threads, 5);
        }
    }

//     @Test(mainClass = BasicApp.class, os = Os.LINUX)
    public void processSamplingWithHigherSampling(TestProcess p) throws Exception {
        p.profile("--proc 5 -d 4 -f %f.jfr");

        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);
            assert events.isEmpty();
        }
    }

//     @Test(mainClass = ShortLivedApp.class, os = Os.LINUX)
    public void shortLivedProcesses(TestProcess p) throws Exception {
        p.profile("--proc 1 -d 6 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);
            assert !events.isEmpty();

            ProcessSample sample = events.stream()
                    .filter(e -> e.name != null && e.name.equals("dd"))
                    .findAny()
                    .orElse(null);

            assert sample != null;
        }
    }

//     @Test(mainClass = ManyProcessApp.class, os = Os.LINUX)
    public void highProcessCount(TestProcess p) throws Exception {
        p.profile("--proc 4 -d 8 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);

            Assert.isLess(events.size(), 5001);
        }
    }

    @Test(mainClass = BasicApp.class, os = Os.MACOS)
    public void processSamplingNotSupportedOnMacOS(TestProcess p) throws Exception {
        try {
            p.profile("--proc 1 -d 3 -f %f.jfr");
            // If we get here, the profiler didn't fail as expected
            assert false;
        } catch (IOException e) {
            // This is expected - process sampling should fail on macOS
            assert e.getMessage().contains("Process sampling is not supported");
        }
    }

    @Test(mainClass = BasicApp.class, os = Os.LINUX)
    public void processSamplingRequiresJfr(TestProcess p) throws Exception {
        try {
            Output out = p.profile("--proc 1 -d 3 -o collapsed");
            assert !out.contains("ProcessSample");
        } catch (IOException e) {
            assert e.getMessage().contains("Process sampling requires JFR output format");
        }
    }

    @Test(mainClass = CpuIntensiveApp.class, os = Os.LINUX)
    public void processSamplingWithCpuThreshold(TestProcess p) throws Exception {
        p.profile("--proc 1 -d 3 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);
            assert !events.isEmpty();

            ProcessSample sample = events.stream()
                    .filter(e -> e.cmdLine != null && e.cmdLine.contains("CpuIntensiveApp"))
                    .max(Comparator.comparingLong(e -> e.time))
                    .orElse(null);

            assert sample != null;
            Assert.isGreater(sample.cpuUser, 0);
            Assert.isGreater(sample.cpuPercent, 0);
            Assert.isGreaterOrEqual(sample.cpuSystem, 0);
        }
    }

    @Test(mainClass = MemoryIntensiveApp.class, jvmVer = {11, Integer.MAX_VALUE}, jvmArgs = "-XX:+AlwaysPreTouch -XX:InitialRAMPercentage=10", os = Os.LINUX)
    public void processSamplingWithMemoryThreshold(TestProcess p) throws Exception {
        // -XX:+AlwaysPreTouch will delay JVM startup, which makes it possible for "kill(pid, SIGQUIT);" in asprof
        // to be called before the JVM installs the signal handlers, which will cause the process to exit early
        Thread.sleep(1000);
        p.profile("--proc 1 -d 2 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);

            ProcessSample sample = events.stream()
                    .filter(e -> e.cmdLine != null && e.cmdLine.contains("MemoryIntensiveApp"))
                    .max(Comparator.comparingLong(e -> e.time))
                    .orElse(null);

            assert sample != null;
            Assert.isGreater(sample.vmSize, 0);
            Assert.isGreater(sample.vmRss, 0);
        }
    }

//     @Test(mainClass = BasicApp.class, os = Os.LINUX)
    public void customSamplingInterval(TestProcess p) throws Exception {
        p.profile("--proc 5 -d 8 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);
            assert !events.isEmpty();
            Assert.isGreaterOrEqual(events.size(), 1);
        }
    }

//     @Test(mainClass = BasicApp.class, os = Os.LINUX)
    public void validateMemoryBreakdown(TestProcess p) throws Exception {
        p.profile("--proc 1 -d 5 -f %f.jfr");
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            List<ProcessSample> events = jfr.readAllEvents(ProcessSample.class);
            assert !events.isEmpty();

            events.forEach(sample -> {
                Assert.isGreaterOrEqual(sample.rssAnon, 0);
                Assert.isGreaterOrEqual(sample.rssFiles, 0);
                Assert.isGreaterOrEqual(sample.rssShmem, 0);
                Assert.isGreaterOrEqual(sample.vmRss, 0);

                if (sample.vmRss > 0) {
                    long totalBreakdown = sample.rssAnon + sample.rssFiles + sample.rssShmem;
                    Assert.isLessOrEqual(totalBreakdown, sample.vmRss * 1.1);
                }
            });
        }
    }
}
