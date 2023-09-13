package test.alloc;

import one.profiler.test.Output;
import one.profiler.test.OAssert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.OsType;
import one.profiler.test.JvmType;

public class AllocTests {

    //Test is broken on OpenJ9
    @Test(mainClass = MapReader.class, jvmArgs = "-XX:+UseG1GC -Xmx1g -Xms1g", jvm = {JvmType.HOTSPOT})
    public void alloc(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed");
        OAssert.contains(out, "G1RemSet::");

        out = p.profile("--alloc 1 -d 3 -o collapsed"); //OpenJ9 breaks for alloc
        OAssert.contains(out, "java/io/BufferedReader.readLine;");
        OAssert.contains(out, "java/lang/String.split;");
        OAssert.contains(out, "java/lang/String.trim;");
        OAssert.contains(out, "java\\.lang\\.String\\[]");
    }

    @Test(mainClass = MapReaderOpt.class, jvmArgs = "-XX:+UseParallelGC -Xmx1g -Xms1g", jvm = {JvmType.ZING, JvmType.HOTSPOT})
    public void allocTotal(TestProcess p) throws Exception {

        Output out = p.profile("-e alloc -d 3 -o collapsed --total");
        assert out.samples( "java.util.HashMap\\$Node\\[]") > 1_000_000;

        out = p.profile("stop -o flamegraph --total");
        OAssert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java.lang.Long'\\)");
        OAssert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java.util.HashMap\\$Node\\[]'\\)");
    }

    @Test(mainClass = Hello.class, agentArgs = "start,event=alloc,alloc=1,cstack=fp,flamegraph,file=%f", jvmArgs = "-XX:+UseG1GC -XX:-UseTLAB")
    public void startup(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        OAssert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'JNI_CreateJavaVM'\\)");
        OAssert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java/lang/ClassLoader.loadClass'\\)");
        OAssert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java\\.lang\\.Class'\\)");
        OAssert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java\\.lang\\.Thread'\\)");
        OAssert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'java\\.lang\\.String'\\)");
        OAssert.contains(out, "f\\(\\d+,\\d+,\\d+,\\d,'int\\[]'\\)");
    }

    @Test(mainClass = MapReaderOpt.class, agentArgs = "start,event=G1CollectedHeap::humongous_obj_allocate", jvmArgs = "-XX:+UseG1GC -XX:G1HeapRegionSize=1M -Xmx4g -Xms4g", os = {OsType.LINUX})
    public void humongous(TestProcess p) throws Exception {
        Thread.sleep(2000);
        Output out = p.profile("stop -o collapsed");
        OAssert.contains(out, "java/io/ByteArrayOutputStream.toByteArray;");
        OAssert.contains(out, "G1CollectedHeap::humongous_obj_allocate");
    }
}
