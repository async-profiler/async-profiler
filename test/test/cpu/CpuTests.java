/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.cpu;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.Os;
import one.profiler.test.Jvm;

public class CpuTests {

    @Test(mainClass = RegularPeak.class, enabled = false, jvmArgs = "-XX:+UseG1GC -Xmx1g -Xms1g", jvm = {Jvm.HOTSPOT})
    public void regularPeak(TestProcess p) throws Exception {
        p.profile("-e cpu -d 3 -f %f.jfr");
        Output out = p.readFile("%f");
        Assert.contains(out, "java/util/stream/SpinedBuffer\\.accept");
    }
}
