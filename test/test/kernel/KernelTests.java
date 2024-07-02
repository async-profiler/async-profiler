package test.kernel;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.OsType;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

public class KernelTests {

    @Test(mainClass = ListFiles.class, os = {OsType.LINUX})
    public void kernel(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -i 1ms -o collapsed");
        Output stderr = p.readPErr();
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

    @Test(mainClass = ListFiles.class, os = {OsType.LINUX})
    public void fdtransfer(TestProcess p) throws Exception {
        p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
        Output out = p.readFile("%f");
        Assert.contains(out, "test/kernel/ListFiles.listFiles;java/io/File");
        Assert.contains(out, "sys_getdents");
    }

    @Test(mainClass = ListFiles.class,  jvmArgs = "-XX:+UseParallelGC -Xmx1g -Xms1g", os = {OsType.MACOS, OsType.WINDOWS})
    public void noLinuxKernel(TestProcess p) throws Exception {
        try {
            p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
            throw new AssertionError("Somehow initialized FdTransferClient with no Linux Kernel???");
        } catch (IOException e) {
            Assert.contains(p.readPErr(), "Failed to initialize FdTransferClient");
        }
    }
}
