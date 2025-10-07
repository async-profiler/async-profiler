/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.c;

import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

import java.io.File;

public class CTests {

    @Test(sh = "%testbin/native_api %api_file.jfr", output = true)
    @Test(sh = "%testbin/native_api %api_file.jfr", env = {"ASPROF_COMMAND=start,cpu,file=%preload_file.jfr"},
            output = true, nameSuffix = "fake-preload")
    public void nativeApi(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;
        assert out.contains("Starting profiler");
        assert out.contains("Stopping profiler");
        assert p.getFile("%api_file").length() > 0;

        File preloadFile = p.getFile("%preload_file");
        assert preloadFile == null || preloadFile.length() == 0;
    }

    @Test(sh = "LD_PRELOAD=%lib %testbin/multiple_profilers preload %profiler_2.jfr", env = {"ASPROF_COMMAND=start,nativemem,file=%profiler_1.jfr"}, nameSuffix = "preload")
    // On macOS dlopen on a copied file can result in the same shared object from a dlopen on the original file
    // otool -D build/test/lib/libasyncProfiler-copy.dylib; otool -D build/lib/libasyncProfiler.dylib
    // Preloading one of the shared objects bypasses this issue, AllocTests.objectSamplerWtihDifferentAsprofs isn't effected by this
    @Test(sh = "LD_PRELOAD=%lib %testbin/multiple_profilers api %profiler_1.jfr %profiler_2.jfr", nameSuffix = "api", os = Os.MACOS)
    @Test(sh = "%testbin/multiple_profilers api %profiler_1.jfr %profiler_2.jfr", nameSuffix = "api", os = Os.LINUX)
    public void twoProfilers(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output preloadProfiler = Output.convertJfrToCollapsed(p.getFilePath("%profiler_1"), "--nativemem");
        Output apiProfiler = Output.convertJfrToCollapsed(p.getFilePath("%profiler_2"), "--wall");

        assert preloadProfiler.contains("doMalloc");
        assert apiProfiler.total() >= 10;
    }
}
