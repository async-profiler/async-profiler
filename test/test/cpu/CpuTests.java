/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.cpu;

import java.io.IOException;

import one.profiler.test.Assert;
import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.Jvm;

public class CpuTests {

    private static void assertCloseTo(long value, long target, String message) {
        Assert.isGreaterOrEqual(value, target * 0.75, message);
        Assert.isLessOrEqual(value, target * 1.25, message);
    }

    private static void pinCpu(TestProcess p, int cpu) throws Exception {
        String[] tasksetCmd = {"taskset", "-acp", String.valueOf(cpu), String.valueOf(p.pid())};
        ProcessBuilder cpuPinPb = new ProcessBuilder(tasksetCmd).inheritIO();
        if (cpuPinPb.start().waitFor() != 0) {
            throw new RuntimeException("Could not set CPU list for the test process");
        }
    }

    @Test(mainClass = CpuBurner.class, os = Os.LINUX)
    public void ctimerTotal(TestProcess p) throws Exception {
        Output out = p.profile("-d 2 -e ctimer -i 100ms --total -o collapsed");
        assertCloseTo(out.total(), 2_000_000_000, "ctimer total should match profiling duration");

        out = p.profile("-d 2 -e ctimer -i 1us --total -o collapsed");
        assertCloseTo(out.total(), 2_000_000_000, "ctimer total should not depend on the profiling interval");
    }

    @Test(mainClass = CpuBurner.class)
    public void itimerTotal(TestProcess p) throws Exception {
        Output out = p.profile("-d 2 -e itimer -i 100ms --total -o collapsed");
        assertCloseTo(out.total(), 2_000_000_000, "itimer total should match profiling duration");
    }

    @Test(mainClass = CpuBurner.class, os = Os.LINUX)
    public void perfEventsWrongTargetCpu(TestProcess p) throws Exception {
        pinCpu(p, 1);

        Output out = p.profile("-d 2 -e cpu -i 100ms --total -o collapsed --target-cpu 0");
        Assert.isEqual(out.total(), 0, "perf_events total should be 0 when the wrong CPU is targeted");
    }

    @Test(mainClass = CpuBurner.class, os = Os.LINUX)
    public void perfEventsRightTargetCpu(TestProcess p) throws Exception {
        pinCpu(p, 0);

        Output out = p.profile("-d 2 -e cpu -i 100ms --total -o collapsed --target-cpu 0");
        assertCloseTo(out.total(), 2_000_000_000, "perf_events total should match profiling duration");
    }

    @Test(mainClass = CpuBurner.class, os = Os.LINUX)
    public void perfEventsWrongTargetCpuWithFdTransfer(TestProcess p) throws Exception {
        pinCpu(p, 1);

        Output out = p.profile("-d 2 -e cpu -i 100ms --total -o collapsed --target-cpu 0 --fdtransfer");
        Assert.isEqual(out.total(), 0, "perf_events total should be 0 when the wrong CPU is targeted");
    }

    @Test(mainClass = CpuBurner.class, os = Os.LINUX)
    public void perfEventsRightTargetCpuWithFdTransfer(TestProcess p) throws Exception {
        pinCpu(p, 0);

        Output out = p.profile("-d 2 -e cpu -i 100ms --total -o collapsed --target-cpu 0 --fdtransfer");
        assertCloseTo(out.total(), 2_000_000_000, "perf_events total should match profiling duration");
    }

    @Test(mainClass = CpuBurner.class, os = Os.LINUX)
    public void itimerDoesNotSupportTargetCpu(TestProcess p) throws Exception {
        try {
            Output out = p.profile("-e itimer --target-cpu 1");
            throw new IllegalStateException("Profiling should have failed");
        } catch (IOException expectedException) {}
    }

    @Test(mainClass = CpuBurner.class, os = Os.LINUX)
    public void ctimerDoesNotSupportTargetCpu(TestProcess p) throws Exception {
        try {
            Output out = p.profile("-e ctimer --target-cpu 1");
            throw new IllegalStateException("Profiling should have failed");
        } catch (IOException expectedException) {}
    }
}
