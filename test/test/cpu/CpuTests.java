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
        long wallTime;
        try (CpuTimeService timeService = new CpuTimeService(p.pid())) {
            wallTime = System.nanoTime();
            cpuTime = timeService.getProcessCpuTimeNanos();
            out = p.profile("-d 2 -e itimer -i 100ms --total -o collapsed");
            cpuTime = timeService.getProcessCpuTimeNanos() - cpuTime;
            wallTime = System.nanoTime() - wallTime;
        }
        double ratio = (double)cpuTime / wallTime;
        long actual = out.total();
        System.out.println("CPU time / wall time ratio: " + ratio);
        System.out.println("itimer total: " + actual / 1_000_000 + " cpu " + cpuTime / 1_000_000 + " wall " + wallTime / 1_000_000 + " expected " + (long)(2_000 * ratio));

        assertCloseTo(actual, (long)(2_000_000_000 * ratio), "itimer total should match CPU time spent in the process during profiling");
    }
}
