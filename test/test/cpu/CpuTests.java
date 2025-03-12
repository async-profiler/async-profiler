/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.cpu;

import one.profiler.test.Assert;
import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class CpuTests {

    private static void assertCloseTo(long value, long target, String message) {
        Assert.isGreaterOrEqual(value, target * 0.75, message);
        Assert.isLessOrEqual(value, target * 1.25, message);
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
        Output out;
        long cpuTime;
        try (CPUTimeService timeService = new CPUTimeService(p.pid())) {
            cpuTime = timeService.getProcessCPUTimeNanos();
            out = p.profile("-d 2 -e itimer -i 100ms --total -o collapsed");
            cpuTime = timeService.getProcessCPUTimeNanos() - cpuTime;
        }
        assertCloseTo(out.total(), cpuTime, "itimer total should match CPU time spent in the process during profiling");
    }
}
