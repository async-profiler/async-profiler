/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.AllocationSample;
import one.profiler.test.*;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
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

    @Test(mainClass = MapReaderOpt.class, jvmVer = {11, Integer.MAX_VALUE})
    public void objectSamplerWtihDifferentAsprofs(TestProcess p) throws Exception {
        Output out = p.profile("-e alloc -d 3 -o collapsed");
        // _[k] suffix in collapsed output corresponds to jdk.ObjectAllocationOutsideTLAB, which means alloc tracer is being used
        assert !out.contains("_\\[k\\]"); // we are using alloc tracer instead of object sampler, should definitely not happen on first profiling call
        File asprofCopy = File.createTempFile(new File(p.profilerLibPath()).getName(), null);
        asprofCopy.deleteOnExit();
        Files.copy(Paths.get(p.profilerLibPath()), asprofCopy.toPath(), StandardCopyOption.REPLACE_EXISTING);
        Output outWithCopy = p.profile(String.format("--libpath %s -e alloc -d 3 -o collapsed", asprofCopy.getAbsolutePath()));
        assert !outWithCopy.contains("_\\[k\\]"); // first instance of profiler has not properly relinquished the can_generate_sampled_object_alloc_events capability.
    }

    // Without liveness tracking, results won't change except for the sampling
    // error.
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = {11, Integer.MAX_VALUE}, args = "1.0", agentArgs = "start,alloc=1k,total,file=%f,collapsed")
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = {11, Integer.MAX_VALUE}, args = "0.0", agentArgs = "start,alloc=1k,total,file=%f,collapsed,live")
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = {11, Integer.MAX_VALUE}, args = "0.1", agentArgs = "start,alloc=1k,total,file=%f,collapsed,live")
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = {11, Integer.MAX_VALUE}, args = "1.0", agentArgs = "start,alloc=1k,total,file=%f,collapsed,live")
    public void liveness(TestProcess p) throws Exception {
        final long TOTAL_BYTES = 50000000;
        final double tolerance = 0.10;

        // keepChance = live ? args() : 1.0, which is equal to args().
        final double keepChance = Double.parseDouble(p.test().args());

        Output out = p.waitForExit("%f");
        long totalBytes = out.filter("RandomBlockRetainer\\.alloc").samples("byte\\[\\]");

        final double lowerLimit = (keepChance - tolerance) * TOTAL_BYTES;
        final double upperLimit = (keepChance + tolerance) * TOTAL_BYTES;

        Assert.isLessOrEqual(lowerLimit, totalBytes);
        Assert.isGreaterOrEqual(upperLimit, totalBytes);
    }

    @Test(mainClass = RandomBlockRetainer.class, jvmVer = {11, Integer.MAX_VALUE}, args = "1.0", agentArgs = "start,alloc=1k,total,file=%f.jfr")
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = {11, Integer.MAX_VALUE}, args = "0.0", agentArgs = "start,alloc=1k,total,file=%f.jfr,live")
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = {11, Integer.MAX_VALUE}, args = "0.1", agentArgs = "start,alloc=1k,total,file=%f.jfr,live")
    @Test(mainClass = RandomBlockRetainer.class, jvmVer = {11, Integer.MAX_VALUE}, args = "1.0", agentArgs = "start,alloc=1k,total,file=%f.jfr,live")
    public void livenessJfrHasStacks(TestProcess p) throws Exception {
        p.waitForExit();
        String filename = p.getFile("%f").toPath().toString();
        try (JfrReader r = new JfrReader(filename)) {
            List<AllocationSample> events = r.readAllEvents(AllocationSample.class);
            assert !events.isEmpty() : "No AllocationSample events found in JFR output";
            for (AllocationSample event : events) {
                StackTrace trace = r.stackTraces.get(event.stackTraceId);
                assert trace != null : "Stack trace missing for id " + event.stackTraceId;
            }
        }
    }
}
