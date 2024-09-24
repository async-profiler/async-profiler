/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.AllocationSample;
import one.jfr.event.Event;
import one.profiler.test.Assert;
import one.profiler.test.Jvm;
import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

import java.util.List;

public class AllocTests {

    @Test(mainClass = MapReader.class, jvmArgs = "-XX:+UseG1GC -Xmx1g -Xms1g", jvm = Jvm.HOTSPOT)
    public void alloc(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed");
        assert out.contains("G1RemSet::");

        out = p.profile("--alloc 1 -d 3 -o collapsed");
        assert out.contains("java/io/BufferedReader.readLine;");
        assert out.contains("java/lang/String.split;");
        assert out.contains("java/lang/String.trim;");
        assert out.contains("java\\.lang\\.String\\[]");
    }

    @Test(mainClass = MapReaderOpt.class, jvmArgs = "-XX:+UseParallelGC -Xmx1g -Xms1g", jvm = {Jvm.HOTSPOT, Jvm.ZING})
    public void allocTotal(TestProcess p) throws Exception {
        Output out = p.profile("-e alloc -d 3 -o collapsed --total");
        assert out.samples("java.util.HashMap\\$Node\\[]") > 1_000_000;

        out = p.profile("stop -o flamegraph --total");
        out = out.convertFlameToCollapsed();
        assert out.contains("java\\.lang\\.Long");
        assert out.contains("java\\.util\\.HashMap\\$Node\\[]");
    }

    @Test(mainClass = Hello.class, agentArgs = "start,event=alloc,alloc=1,cstack=fp,flamegraph,file=%f", jvmArgs = "-XX:+UseG1GC -XX:-UseTLAB")
    public void startup(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        out = out.convertFlameToCollapsed();
        assert out.contains("JNI_CreateJavaVM");
        assert out.contains("java/lang/ClassLoader\\.loadClass");
        assert out.contains("java\\.lang\\.Class");
        assert out.contains("java\\.lang\\.Thread");
        assert out.contains("java\\.lang\\.String");
        assert out.contains("int\\[]");
    }

    @Test(mainClass = MapReaderOpt.class, agentArgs = "start,event=G1CollectedHeap::humongous_obj_allocate", jvmArgs = "-XX:+UseG1GC -XX:G1HeapRegionSize=1M -Xmx4g -Xms4g", os = Os.LINUX)
    public void humongous(TestProcess p) throws Exception {
        Thread.sleep(1000);
        Output out = p.profile("stop -o collapsed");
        assert out.contains("java/io/ByteArrayOutputStream.toByteArray;");
        assert out.contains("G1CollectedHeap::humongous_obj_allocate");
    }

    // Without liveness tracking, results won't change except for the sampling
    // error.
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = { 11, Integer.MAX_VALUE }, args = "1.0", inputs = { "false" })
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = { 11, Integer.MAX_VALUE }, args = "0.0", inputs = { "true" })
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = { 11, Integer.MAX_VALUE }, args = "0.1", inputs = { "true" })
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = { 11, Integer.MAX_VALUE }, args = "1.0", inputs = { "true" })
    public void liveness(TestProcess p) throws Exception {
        final long TOTAL_BYTES = 50000000;
        final double tolerance = 0.10;

        final boolean live = Boolean.parseBoolean(p.inputs()[0]);
        final double keepChance = live ? Double.parseDouble(p.test().args()) : 1.0;

        Output out = p.profile("--alloc 1k --total -o collapsed" + (live ? " --live" : ""));
        long totalBytes = out.filter("RandomBlockRetainer\\.alloc").samples("byte\\[\\]");

        final double lowerLimit = (keepChance - tolerance) * TOTAL_BYTES;
        final double upperLimit = (keepChance + tolerance) * TOTAL_BYTES;

        Assert.isLessOrEqual(lowerLimit, totalBytes);
        Assert.isGreaterOrEqual(upperLimit, totalBytes);
    }
}
