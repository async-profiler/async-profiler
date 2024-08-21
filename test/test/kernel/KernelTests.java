/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.kernel;

import one.convert.Arguments;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.Os;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

import static one.profiler.test.ProfileOutputType.COLLAPSED;
import static one.profiler.test.ProfileOutputType.FLAMEGRAPH;

public class KernelTests {

    @Test(mainClass = ListFiles.class, os = Os.LINUX)
    public void kernel(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -i 1ms -o collapsed");
        Output err = p.readFile(TestProcess.PROFERR);
        assert out.contains("test/kernel/ListFiles.listFiles;java/io/File");
        assert err.contains("Kernel symbols are unavailable") || out.contains("sys_getdents");

        out = p.profile("stop -o flamegraph");
        String convertedOut = out.convert(
                FLAMEGRAPH, COLLAPSED, new ByteArrayInputStream(out.toString().getBytes(StandardCharsets.UTF_8)));
        assert convertedOut.contains("java/io/File.list");
        assert err.contains("Kernel symbols are unavailable") || convertedOut.contains("sys_getdents");
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
