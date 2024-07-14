/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.cpu;

import one.profiler.test.Jvm;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class CpuTests {

    @Test(mainClass = RegularPeak.class, enabled = false, jvmArgs = "-XX:+UseG1GC -Xmx1g -Xms1g", jvm = Jvm.HOTSPOT)
    public void regularPeak(TestProcess p) throws Exception {
        p.profile("-e cpu -d 3 -f %f.jfr");
        Output out = p.readFile("%f");
        assert out.contains("java/util/stream/SpinedBuffer\\.accept");
    }
}
