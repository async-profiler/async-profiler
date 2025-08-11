/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfrconverter;

import one.convert.*;
import one.profiler.test.*;

// Simple smoke tests for JFR converter. The output is not inspected for errors,
// we only verify that the conversion completes successfully.
public class JfrconverterTests {

    @Test(mainClass = CpuBurner.class, agentArgs = "start,jfr,file=profile.jfr,all,file=%f")
    public void allocationHeatmapConversion(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        JfrToHeatmap.convert("profile.jfr", "/dev/null", new Arguments("--alloc"));
    }

    @Test(sh = "")
    public void cpuHeatmapConversion(TestProcess p) throws Exception {
        JfrToHeatmap.convert("profile.jfr", "/dev/null", new Arguments("--cpu"));
    }

    @Test(sh = "")
    public void flamegraphConversion(TestProcess p) throws Exception {
        JfrToFlame.convert("profile.jfr", "/dev/null", new Arguments());
    }
}
