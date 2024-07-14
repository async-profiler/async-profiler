/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.kernel;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.Os;

import java.io.IOException;

public class KernelTests {

    @Test(mainClass = ListFiles.class, os = Os.LINUX)
    public void kernel(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -i 1ms -o collapsed");
        Output err = p.readFile(TestProcess.PROFERR);
        assert out.contains("test/kernel/ListFiles.listFiles;java/io/File");
        if (!err.contains("Kernel symbols are unavailable")) {
            assert out.contains("sys_getdents");
        }

        out = p.profile("stop -o flamegraph");
//        assert out.contains("f\\(\\d+,\\d+,\\d+,\\d,'java/io/File.list'(,\\d+,\\d+,\\d+)?\\)");
//        if (!err.contains("Kernel symbols are unavailable")) {
//            assert out.contains("sys_getdents");
//        }
    }

    @Test(mainClass = ListFiles.class, os = Os.LINUX)
    public void fdtransfer(TestProcess p) throws Exception {
        p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
        Output out = p.readFile("%f");
        assert out.contains("test/kernel/ListFiles.listFiles;java/io/File");
        assert out.contains("sys_getdents");
    }

    @Test(mainClass = ListFiles.class, jvmArgs = "-XX:+UseParallelGC -Xmx1g -Xms1g", os = {Os.MACOS, Os.WINDOWS})
    public void notLinux(TestProcess p) throws Exception {
        try {
            p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
            throw new AssertionError("FdTransferClient should succeed on Linux only");
        } catch (IOException e) {
            assert p.readFile(TestProcess.PROFERR).contains("Failed to initialize FdTransferClient");
        }
    }
}
