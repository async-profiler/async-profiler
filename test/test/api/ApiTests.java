/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class ApiTests {

    @Test(mainClass = DumpCollapsed.class, jvmArgs = "-Djava.library.path=build/lib", output = true)
    public void flat(TestProcess p) throws Exception {
        Output out = p.waitForExit("%pout");
        Assert.contains(out, "BusyLoops.method1;");
        Assert.contains(out, "BusyLoops.method2;");
        Assert.contains(out, "BusyLoops.method3;");
    }

    @Test(mainClass = StopResume.class, jvmArgs = "-Djava.library.path=build/lib", output = true)
    public void stopResume(TestProcess p) throws Exception {
        Output out = p.waitForExit("%pout");
        Assert.notContains(out, "BusyLoops.method1");
        Assert.contains(out, "BusyLoops.method2");
        Assert.notContains(out, "BusyLoops.method3");
    }
}
