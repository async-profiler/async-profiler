/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nonjava;

import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class NonjavaTests {

    @Test(sh = "%testbin/non_java_app 1 %s.html", output = true)
    public void jvmLoadedBeforeSessionStartTest(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;

        out = p.readFile("%s");
        assert out.contains(".cpuHeavyTask");
    }

    @Test(sh = "%testbin/non_java_app 2 %s.html", output = true)
    public void jvmLoadedAfterSessionStartTest(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;

        out = p.readFile("%s");
        assert !out.contains(".cpuHeavyTask");
    }

    @Test(sh = "%testbin/non_java_app 3 %f.html %s.html", output = true)
    public void jvmLoadedAfterFirstSessionButBeforeSecondSessionTest(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;

        out = p.readFile("%f");
        assert !out.contains(".cpuHeavyTask");

        out = p.readFile("%s");
        assert out.contains(".cpuHeavyTask");
    }
}
