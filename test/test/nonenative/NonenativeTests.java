/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nonenative;

import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class NonenativeTests {

    @Test(sh = "%testbin/java_first_none_native_app %s.html", output = true)
    public void javaFirstNoneNativeAppTest(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;

        out = p.readFile("%s");
        assert out.contains(".cpuHeavyTask");
    }

    // TODO: Make work on MacOs (Problem in symbol parsing [symbols_macos.cpp])
    @Test(sh = "%testbin/async_profiler_first_none_native_app %s.html", output = true, os = Os.LINUX)
    public void asyncProfilerFirstNoneNativeAppTest(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;

        out = p.readFile("%s");
        assert out.contains(".cpuHeavyTask");
    }
}
