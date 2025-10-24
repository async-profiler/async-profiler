/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class ApiTests {

    @Test(mainClass = DumpCollapsed.class, output = true)
    public void flat(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert out.contains("BusyLoops.method1;");
        assert out.contains("BusyLoops.method2;");
        assert out.contains("BusyLoops.method3;");
    }

    @Test(mainClass = DumpOtlp.class)
    public void otlp(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
    }

    @Test(mainClass = StopResume.class, output = true)
    public void stopResume(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert !out.contains("BusyLoops.method1");
        assert out.contains("BusyLoops.method2");
        assert !out.contains("BusyLoops.method3");
    }

    // https://github.com/async-profiler/async-profiler/issues/1564
    @Test(mainClass = Version.class, output = true)
    public void version(TestProcess p) throws Exception {
        String actual = p.waitForExit(TestProcess.STDOUT).toString().trim();
        String expected = System.getenv("PROFILER_VERSION");
        assert expected.equals(actual) : String.format("%s != %s", expected, actual);
    }
}
