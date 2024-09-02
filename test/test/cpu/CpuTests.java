/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.cpu;

import one.convert.Arguments;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class CpuTests {

    @Test(mainClass = RegularPeak.class)
    public void regularPeakLinux(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 6 -f %f.jfr");
        String jfrOutPath = p.getFile("%f").getAbsolutePath();
        out = out.convertJfrToCollapsed(jfrOutPath, new Arguments("-o", "collapsed", "--to", "2500"));
        assert !out.contains("test/cpu/Cache\\.lambda\\$calculateTop\\$1");
        out = out.convertJfrToCollapsed(jfrOutPath, new Arguments("-o", "collapsed", "--from", "2500", "--to", "5000"));
        assert out.samples("test/cpu/Cache\\.lambda\\$calculateTop\\$1") >= 1;
        out = out.convertJfrToCollapsed(jfrOutPath, new Arguments("-o", "collapsed", "--from", "5000"));
        assert !out.contains("test/cpu/Cache\\.lambda\\$calculateTop\\$1");
    }
}
