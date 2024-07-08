/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.kernel;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.Os;

import java.io.IOException;

import static one.profiler.test.TestConstants.PROFILE_ERROR_FIELD;
import static one.profiler.test.TestConstants.PROFILE_OUTPUT_FIELD;

public class KernelTests {

    @Test(mainClass = ListFiles.class, enabled = false, os = {Os.LINUX})
    public void kernel(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -i 1ms -o collapsed");
        Output stderr = p.readFile(PROFILE_ERROR_FIELD);
        Assert.contains(out, "test/kernel/ListFiles.listFiles;java/io/File");
        if (!stderr.contains("Kernel symbols are unavailable")) {
            Assert.contains(stderr, "sys_getdents");
        }
        out = p.profile("stop -o flamegraph");
        Assert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java/io/File.list'(,\\d+,\\d+,\\d+)?\\)");
        if (!stderr.contains("Kernel symbols are unavailable")) {
            Assert.contains(out, "sys_getdents");
        }
    }

    @Test(mainClass = ListFiles.class, os = {Os.LINUX})
    public void fdtransfer(TestProcess p) throws Exception {
        p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
        Output out = p.readFile("%f");
        Assert.contains(out, "test/kernel/ListFiles.listFiles;java/io/File");
        Assert.contains(out, "sys_getdents");
    }

    @Test(mainClass = ListFiles.class, jvmArgs = "-XX:+UseParallelGC -Xmx1g -Xms1g", os = {Os.MACOS, Os.WINDOWS})
    public void noLinuxKernel(TestProcess p) throws Exception {
        try {
            p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
            throw new AssertionError("Somehow initialized FdTransferClient with no Linux Kernel???");
        } catch (IOException e) {
            Assert.contains(p.readFile(PROFILE_ERROR_FIELD), "Failed to initialize FdTransferClient");
        }
    }
}
