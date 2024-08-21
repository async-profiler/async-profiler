/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.Os;
import one.profiler.test.Jvm;

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
        assert out.contains("java\\.lang\\.Long");
        assert out.contains("java\\.util\\.HashMap\\$Node\\[]");
    }

    @Test(mainClass = Hello.class, enabled = false, agentArgs = "start,event=alloc,alloc=1,cstack=fp,flamegraph,file=%f", jvmArgs = "-XX:+UseG1GC -XX:-UseTLAB")
    public void startup(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert out.contains("f\\(\\d+,\\d+,\\d+,\\d,'JNI_CreateJavaVM'\\)");
        assert out.contains("f\\(\\d+,\\d+,\\d+,\\d,'java/lang/ClassLoader.loadClass'\\)");
        assert out.contains("f\\(\\d+,\\d+,\\d+,\\d,'java\\.lang\\.Class'\\)");
        assert out.contains("f\\(\\d+,\\d+,\\d+,\\d,'java\\.lang\\.Thread'\\)");
        assert out.contains("f\\(\\d+,\\d+,\\d+,\\d,'java\\.lang\\.String'\\)");
        assert out.contains("f\\(\\d+,\\d+,\\d+,\\d,'int\\[]'\\)");
    }

    @Test(mainClass = MapReaderOpt.class, agentArgs = "start,event=G1CollectedHeap::humongous_obj_allocate", jvmArgs = "-XX:+UseG1GC -XX:G1HeapRegionSize=1M -Xmx4g -Xms4g", os = Os.LINUX)
    public void humongous(TestProcess p) throws Exception {
        Thread.sleep(1000);
        Output out = p.profile("stop -o collapsed");
        assert out.contains("java/io/ByteArrayOutputStream.toByteArray;");
        assert out.contains("G1CollectedHeap::humongous_obj_allocate");
    }
}
