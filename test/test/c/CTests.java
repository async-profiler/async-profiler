/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.c;

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
}
