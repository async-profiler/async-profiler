/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.c;

import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class CTests {

    // TODO: Make the test work on macOS
    @Test(sh = {"gcc -Isrc test/test/c/nativeApi.c -ldl -o%c", "%c %f.jfr"}, output = true, os = Os.LINUX)
    public void nativeApi(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;
        assert out.contains("Starting profiler");
        assert out.contains("Stopping profiler");
        assert p.getFile("%f").length() > 0;
    }
}
