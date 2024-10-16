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
        assert err.contains("Kernel symbols are unavailable") || out.contains("sys_getdents");

        out = p.profile("stop -o flamegraph");
        out = out.convertFlameToCollapsed();
        assert out.contains("java/io/File.list");
        assert err.contains("Kernel symbols are unavailable") || out.contains("sys_getdents");
    }

    @Test(mainClass = ListFiles.class, os = Os.LINUX)
    public void fdtransfer(TestProcess p) throws Exception {
        p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
        Output out = p.readFile("%f");
        assert out.contains("test/kernel/ListFiles.listFiles;java/io/File");
        assert out.contains("sys_getdents");
    }

    @Test(mainClass = ListFiles.class, os = {Os.MACOS, Os.WINDOWS})
    public void notLinux(TestProcess p) throws Exception {
        try {
            p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer");
            throw new AssertionError("FdTransferClient should succeed on Linux only");
        } catch (IOException e) {
            assert p.readFile(TestProcess.PROFERR).contains("Failed to initialize FdTransferClient");
        }
    }
}
