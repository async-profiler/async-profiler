/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.Os;
import one.profiler.test.Jvm;

public class AllocTests {

    //Test is broken on OpenJ9
    @Test(mainClass = MapReader.class, jvmArgs = "-XX:+UseG1GC -Xmx1g -Xms1g", jvm = {Jvm.HOTSPOT})
    public void alloc(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed");
        Assert.contains(out, "G1RemSet::");

        out = p.profile("--alloc 1 -d 3 -o collapsed"); //OpenJ9 breaks for alloc
        Assert.contains(out, "java/io/BufferedReader.readLine;");
        Assert.contains(out, "java/lang/String.split;");
        Assert.contains(out, "java/lang/String.trim;");
        Assert.contains(out, "java\\.lang\\.String\\[]");
    }

    @Test(mainClass = MapReaderOpt.class, jvmArgs = "-XX:+UseParallelGC -Xmx1g -Xms1g", jvm = {Jvm.ZING, Jvm.HOTSPOT})
    public void allocTotal(TestProcess p) throws Exception {
        Output out = p.profile("-e alloc -d 3 -o collapsed --total");
        assert out.samples("java.util.HashMap\\$Node\\[]") > 1_000_000;

        /* Commented out as part of disabling asserting JFR output.
        out = p.profile("stop -o flamegraph --total");
        Assert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java.lang.Long'\\)");
        Assert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java.util.HashMap\\$Node\\[]'\\)");
        */
    }

    @Test(mainClass = Hello.class, enabled = false, agentArgs = "start,event=alloc,alloc=1,cstack=fp,flamegraph,file=%f", jvmArgs = "-XX:+UseG1GC -XX:-UseTLAB")
    public void startup(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        Assert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'JNI_CreateJavaVM'\\)");
        Assert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java/lang/ClassLoader.loadClass'\\)");
        Assert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java\\.lang\\.Class'\\)");
        Assert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java\\.lang\\.Thread'\\)");
        Assert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java\\.lang\\.String'\\)");
        Assert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'int\\[]'\\)");
    }

    @Test(mainClass = MapReaderOpt.class, agentArgs = "start,event=G1CollectedHeap::humongous_obj_allocate", jvmArgs = "-XX:+UseG1GC -XX:G1HeapRegionSize=1M -Xmx4g -Xms4g", os = {Os.LINUX})
    public void humongous(TestProcess p) throws Exception {
        Thread.sleep(1000);
        Output out = p.profile("stop -o collapsed");
        Assert.contains(out, "java/io/ByteArrayOutputStream.toByteArray;");
        Assert.contains(out, "G1CollectedHeap::humongous_obj_allocate");
    }
}
