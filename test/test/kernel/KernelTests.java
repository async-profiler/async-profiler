package test.kernel;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.OsType;
import one.profiler.test.CommandFail;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

public class KernelTests {

    @Test(mainClass = ListFiles.class, os = {OsType.LINUX})
    public void kernel(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -i 1ms -o collapsed --fdtransfer", true);
        out.assertContains("test/kernel/ListFiles.listFiles;java/io/File");
        out.assertContains("sys_getdents");

        out = p.profile("stop -o flamegraph", true);
        out.assertContains("f\\(\\d+,\\d+,\\d+,\\d,'java/io/File.list'(,1,0,0)?\\)");
        out.assertContains("sys_getdents");
    }

    @Test(mainClass = ListFiles.class, os = {OsType.LINUX})
    public void fdtransfer(TestProcess p) throws Exception {
        p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
        Output out = p.readFile("%f");
        out.assertContains("test/kernel/ListFiles.listFiles;java/io/File");
        out.assertContains("sys_getdents");
    }

    @Test(mainClass = ListFiles.class,  jvmArgs = "-XX:+UseParallelGC -Xmx1g -Xms1g", os = {OsType.MACOS, OsType.WINDOWS})
    public void noLinuxKernel(TestProcess p) throws Exception {
        Thread.sleep(1000);
        try {
            p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
        } catch (CommandFail e) {
            e.getStderr().assertContains("Failed to initialize FdTransferClient");
        }
    }
}
