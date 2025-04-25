/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativemem;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import one.jfr.JfrReader;
import one.jfr.event.MallocEvent;

import one.profiler.test.Assert;
import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class NativememTests {

    private static final int MALLOC_SIZE = 1999993;
    private static final int MALLOC_DYN_SIZE = 2000003;
    private static final int CALLOC_SIZE = 2000147;
    private static final int REALLOC_SIZE = 30000170;
    private static final int POSIX_MEMALIGN_SIZE = 30000193;
    private static final int ALIGNED_ALLOC_SIZE = 30002009;

    @Test(mainClass = CallsMallocCalloc.class, os = Os.LINUX, agentArgs = "start,nativemem,total,collapsed,file=%f", args = "once")
    public void canAgentTraceMallocCalloc(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");

        Assert.isEqual(out.samples("Java_test_nativemem_Native_malloc"), MALLOC_SIZE);
        Assert.isEqual(out.samples("Java_test_nativemem_Native_calloc"), CALLOC_SIZE);
    }

    @Test(mainClass = CallsMallocCalloc.class, os = Os.LINUX, agentArgs = "start,nativemem=10000000,total,collapsed,file=%f", args = "once")
    public void canAgentFilterMallocCalloc(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        Assert.isEqual(out.samples("Java_test_nativemem_Native_malloc"), 0);
        Assert.isEqual(out.samples("Java_test_nativemem_Native_calloc"), 0);
    }

    @Test(mainClass = CallsMallocCalloc.class, os = Os.LINUX)
    public void canAsprofTraceMallocCalloc(TestProcess p) throws Exception {
        Output out = p.profile("-e nativemem --total -o collapsed -d 2");
        long samplesMalloc = out.samples("Java_test_nativemem_Native_malloc");
        long samplesCalloc = out.samples("Java_test_nativemem_Native_calloc");

        Assert.isGreater(samplesMalloc, 0);
        Assert.isGreater(samplesCalloc, 0);
        Assert.isEqual(samplesMalloc % MALLOC_SIZE, 0);
        Assert.isEqual(samplesCalloc % CALLOC_SIZE, 0);
    }

    @Test(mainClass = CallsRealloc.class, agentArgs = "start,nativemem,total,collapsed,file=%f", args = "once", os = Os.LINUX)
    public void canAgentTraceRealloc(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");

        Assert.isEqual(out.samples("Java_test_nativemem_Native_malloc"), MALLOC_SIZE);
        Assert.isEqual(out.samples("Java_test_nativemem_Native_realloc"), REALLOC_SIZE);
    }

    @Test(mainClass = CallsRealloc.class, os = Os.LINUX)
    public void canAsprofTraceRealloc(TestProcess p) throws Exception {
        Output out = p.profile("-e nativemem --total -o collapsed -d 2");
        long samplesMalloc = out.samples("Java_test_nativemem_Native_malloc");
        long samplesRealloc = out.samples("Java_test_nativemem_Native_realloc");

        Assert.isGreater(samplesMalloc, 0);
        Assert.isGreater(samplesRealloc, 0);
        Assert.isEqual(samplesMalloc % MALLOC_SIZE, 0);
        Assert.isEqual(samplesRealloc % REALLOC_SIZE, 0);
    }

    @Test(mainClass = CallsAllNoLeak.class, os = Os.LINUX)
    public void canAsprofTraceAllNoLeak(TestProcess p) throws Exception {
        Output out = p.profile("-e nativemem --total -o collapsed -d 2");

        long samplesMalloc = out.samples("Java_test_nativemem_Native_malloc");
        long samplesCalloc = out.samples("Java_test_nativemem_Native_calloc");
        long samplesRealloc = out.samples("Java_test_nativemem_Native_realloc");
        long samplesPosixMemalign = out.samples("Java_test_nativemem_Native_posixMemalign");
        long samplesAlignedAlloc = out.samples("Java_test_nativemem_Native_alignedAlloc");

        Assert.isGreater(samplesMalloc, 0);
        Assert.isGreater(samplesCalloc, 0);
        Assert.isGreater(samplesRealloc, 0);
        Assert.isGreater(samplesPosixMemalign, 0);
        Assert.isGreater(samplesAlignedAlloc, 0);

        Assert.isEqual(samplesMalloc % MALLOC_SIZE, 0);
        Assert.isEqual(samplesCalloc % CALLOC_SIZE, 0);
        Assert.isEqual(samplesRealloc % REALLOC_SIZE, 0);
        Assert.isEqual(samplesPosixMemalign % POSIX_MEMALIGN_SIZE, 0);
        Assert.isEqual(samplesAlignedAlloc % ALIGNED_ALLOC_SIZE, 0);
    }

    private static Map<Long, Long> assertNoLeaks(TestProcess p) throws Exception {
        p.waitForExit();
        String filename = p.getFilePath("%f");

        boolean nofree = Arrays.asList(p.inputs()).contains("nofree");
        boolean hasFree = false;
        Map<Long, Long> sizeCounts = new HashMap<>();

        try (JfrReader r = new JfrReader(filename)) {
            List<MallocEvent> events = r.readAllEvents(MallocEvent.class);
            assert !events.isEmpty() : "No MallocEvent events found in the JFR output";

            long totalAllocated = 0;
            Map<Long, MallocEvent> addresses = new HashMap<>();
            for (MallocEvent event : events) {
                // only interested in specific sizes.
                if (event.size != 0 && event.size != MALLOC_SIZE && event.size != CALLOC_SIZE
                        && event.size != REALLOC_SIZE && event.size != MALLOC_DYN_SIZE
                        && event.size != POSIX_MEMALIGN_SIZE && event.size != ALIGNED_ALLOC_SIZE) {
                    continue;
                }

                totalAllocated += event.size;
                if (event.size > 0) {
                    addresses.put(event.address, event);
                    sizeCounts.merge(event.size, 1L, Long::sum);
                } else {
                    addresses.remove(event.address);
                    hasFree = true;
                }
            }

            Assert.isGreater(totalAllocated, 0);

            if (nofree) {
                assert !hasFree;

                // nofree cannot track leaks.
                Assert.isGreater(addresses.size(), 0);
            } else {
                assert hasFree;
                Assert.isEqual(addresses.size(), 0);
            }
        }

        return sizeCounts;
    }

    @Test(mainClass = CallsAllNoLeak.class, os = Os.LINUX, args = "once", agentArgs = "start,nativemem,file=%f.jfr")
    @Test(mainClass = CallsAllNoLeak.class, os = Os.LINUX, args = "once", agentArgs = "start,nativemem,total,file=%f.jfr")
    @Test(mainClass = CallsAllNoLeak.class, os = Os.LINUX, args = "once", agentArgs = "start,nativemem=1,total,file=%f.jfr")
    @Test(mainClass = CallsAllNoLeak.class, os = Os.LINUX, args = "once", agentArgs = "start,nativemem=10M,total,file=%f.jfr")
    @Test(mainClass = CallsAllNoLeak.class, os = Os.LINUX, args = "once", agentArgs = "start,cpu,alloc,nativemem,total,file=%f.jfr")
    public void jfrNoLeaks(TestProcess p) throws Exception {
        assertNoLeaks(p);
    }

    @Test(mainClass = CallsAllNoLeak.class, os = Os.LINUX, args = "once", inputs = "nofree", agentArgs = "start,nativemem,nofree,file=%f.jfr")
    @Test(mainClass = CallsAllNoLeak.class, os = Os.LINUX, args = "once", inputs = "nofree", agentArgs = "start,cpu,alloc,nativemem,nofree,total,file=%f.jfr")
    public void jfrNoFree(TestProcess p) throws Exception {
        assertNoLeaks(p);
    }

    @Test(mainClass = CallsAllNoLeak.class, os = Os.LINUX, args = "once", env = {"LD_PRELOAD=%lib", "ASPROF_COMMAND=start,nativemem,file=%f.jfr"})
    public void ldpreload(TestProcess p) throws Exception {
        assertNoLeaks(p);

        Output out = Output.convertJfrToCollapsed(p.getFilePath("%f"), "--nativemem");
        assert out.contains("JavaMain");
        assert out.contains("JVM_");
        assert out.contains("malloc_hook");
    }

    @Test(sh = "LD_PRELOAD=%lib ASPROF_COMMAND=start,nativemem,file=%f.jfr %testbin/malloc_plt_dyn", os = Os.LINUX)
    public void malloc_plt_dyn(TestProcess p) throws Exception {
        Map<Long, Long> sizeCounts = assertNoLeaks(p);

        Assert.isEqual(sizeCounts.getOrDefault((long) MALLOC_SIZE, 0L), 1);
        Assert.isEqual(sizeCounts.getOrDefault((long) MALLOC_DYN_SIZE, 0L), 1);
    }

    @Test(os = Os.LINUX, sh = "%testbin/profile_with_dlopen dlopen_first %f.jfr", output = true, env = {"LD_LIBRARY_PATH=build/test/lib:build/lib"}, nameSuffix = "dlopen_first")
    @Test(os = Os.LINUX, sh = "%testbin/profile_with_dlopen profile_first %f.jfr", output = true, env = {"LD_LIBRARY_PATH=build/test/lib:build/lib"}, nameSuffix = "profile_first")
    @Test(os = Os.LINUX, sh = "LD_PRELOAD=%lib %testbin/profile_with_dlopen dlopen_first %f.jfr", output = true, env = {"LD_LIBRARY_PATH=build/test/lib:build/lib"}, nameSuffix = "dlopen_first+LD_PRELOAD")
    @Test(os = Os.LINUX, sh = "LD_PRELOAD=%lib %testbin/profile_with_dlopen profile_first %f.jfr", output = true, env = {"LD_LIBRARY_PATH=build/test/lib:build/lib"}, nameSuffix = "profile_first+LD_PRELOAD")
    public void dlopenCustomLib(TestProcess p) throws Exception {
        Map<Long, Long> sizeCounts = assertNoLeaks(p);

        Assert.isEqual(sizeCounts.getOrDefault((long) MALLOC_SIZE, 0L), 1);
    }
}
