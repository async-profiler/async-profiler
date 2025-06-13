/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.control;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

import java.io.File;
import java.util.Scanner;

public class ControlTests {
    @Test(sh = "%testbin/start_control %api_file.jfr", env = {"ASPROF_COMMAND=start,cpu,file=%preload_file.jfr"}, nameSuffix="fake-preload")
    @Test(sh = "%testbin/start_control %api_file.jfr", nameSuffix="normal")
    public void earlyStart(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output output = Output.convertJfrToCollapsed(p.getFilePath("%api_file"), "--nativemem");
        assert output.contains("main;malloc_hook");

        // File will not exist in process map for all test cases
        File file = p.getFile("%preload_file");
        if (file == null) {
            return;
        }
        Scanner reader = new Scanner(file);
        assert !reader.hasNext();
    }
}
