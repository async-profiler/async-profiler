/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nonjava;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class NonjavaTests {

    // jvm is loaded before the profiling session is started
    @Test(sh = "%testbin/non_java_app 1 %s.html", output = true)
    public void jvmFirstTest(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;

        out = p.readFile("%s");
        assert out.contains(".cpuHeavyTask");
    }

    // jvm is loaded after the profiling session is started
    @Test(sh = "%testbin/non_java_app 2 %s.html", output = true)
    public void profilerFirstTest(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;

        out = p.readFile("%s");
        assert !out.contains(".cpuHeavyTask");
    }

    // jvm is loaded between two profiling sessions
    @Test(sh = "%testbin/non_java_app 3 %f.html %s.html", output = true)
    public void jvmInBetweenTest(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;

        out = p.readFile("%f");
        assert !out.contains(".cpuHeavyTask");

        out = p.readFile("%s");
        assert out.contains(".cpuHeavyTask");
    }
}
