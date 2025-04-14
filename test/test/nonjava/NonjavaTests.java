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
    public void jvmFirst(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%s");
        assert out.contains(".cpuHeavyTask");
    }

    // jvm is loaded after the profiling session is started
    @Test(sh = "%testbin/non_java_app 2 %s.html", output = true)
    public void profilerFirst(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%s");
        assert !out.contains(".cpuHeavyTask");
    }

    // jvm is loaded between two profiling sessions
    @Test(sh = "%testbin/non_java_app 3 %f.html %s.html", output = true)
    public void jvmInBetween(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%f");
        assert !out.contains(".cpuHeavyTask");

        out = p.readFile("%s");
        assert out.contains(".cpuHeavyTask");
    }

    // jvm is loaded before the profiling session is started on a different thread
    @Test(sh = "%testbin/non_java_app 4 %s.html", output = true)
    public void differentThread(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%s");
        assert out.contains(".cpuHeavyTask");
    }
}
