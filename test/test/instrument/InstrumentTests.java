/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.instrument;

import one.profiler.test.*;

import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordingFile;
import java.time.Duration;
import java.io.*;
import java.nio.file.Files;
import java.util.HashMap;


// Append '-Xlog:redefine+class+exceptions*' to jvmArgs for more detailed
// reports from verification errors.
public class InstrumentTests {

    private static final String MAIN_METHOD_SEGMENT = "^test\\/instrument\\/Recursive\\.main";
    private static final String RECURSIVE_METHOD_SEGMENT = ";test\\/instrument\\/Recursive\\.recursive";

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,event=test.instrument.CpuBurner.burn,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void instrument(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assertAllCallsRecorded(out);
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,event=test.instrument.CpuBurner.burn(Ljava/time/Duration;)V,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void instrumentSignature(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assertAllCallsRecorded(out);
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,event=test.instrument.CpuBurner.burn()V,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void instrumentWrongSignature(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert !out.contains("CpuBurner\\.burn");
    }

    // Smoke test: if any validation failure happens Instrument::BytecodeRewriter has a bug
    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,event=*.*,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void instrumentAll(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert out.contains("java\\/lang\\/Thread\\.run ");
        assert out.contains("java\\/lang\\/String\\.<init> ");
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,event=*.<init>,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void instrumentAllInit(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert !out.contains("java\\/lang\\/Thread\\.run ");
        assert out.contains("java\\/lang\\/Thread\\.<init> ");
        assert out.contains("java\\/lang\\/String\\.<init> ");
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,event=java.lang.*.<init>,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void instrumentAllJavaLangInit(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert !out.contains("java\\/lang\\/Thread\\.run ");
        assert out.contains("java\\/lang\\/Thread\\.<init> ");
        assert out.contains("java\\/lang\\/String\\.<init> ");
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,event=java.lang.Thread.<in*,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void instrumentThreadInitWildcard(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert !out.contains("java\\/lang\\/Thread\\.run ");
        assert !out.contains("java\\/lang\\/String\\.<init> ");
        assert out.contains("java\\/lang\\/Thread\\.<init> ");
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,event=java.lang.Thread.*,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void instrumentAllMethodsInClass(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert out.contains("java\\/lang\\/Thread\\.run ");
        assert out.contains("java\\/lang\\/Thread\\.<init> ");
        assert !out.contains("java\\/lang\\/String\\.<init> ");
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,event=test.instrument.CpuBurner.burn,jfr,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void instrumentJfr(TestProcess p) throws Exception {
        p.waitForExit();
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        boolean found = false;
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                assert !event.getEventType().getName().equals("jdk.MethodTrace") : "Should not contain jdk.MethodTrace events";
                if (event.getEventType().getName().equals("jdk.ExecutionSample")) {
                    found = true;
                }
            }
        }
        assert found : "jdk.ExecutionSample not found";
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,trace=test.instrument.CpuBurner.burn:100ms,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void latency(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert out.samples("\\[thread1 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$0;test\\/instrument\\/CpuBurner\\.burn") == 1;
        assert out.samples("\\[thread2 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$1;test\\/instrument\\/CpuBurner\\.burn") == 2;
        assert !out.contains("\\[thread3.*");
        assert !out.contains("\\[thread4.*");
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,trace=test.instrument.CpuBurner.burn:100ms,interval=2,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void latencyAndInterval(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert out.samples(".*;test\\/instrument\\/CpuBurner\\.burn") == 1;
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,trace=test.instrument.CpuBurner.burn,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void latencyZero(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assertAllCallsRecorded(out);
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,trace=test.instrument.CpuBurner.burn(Ljava/time/Duration;)V,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void latencySignature(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assertAllCallsRecorded(out);
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,trace=test.instrument.CpuBurner.burn()V,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void latencyWrongSignature(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert !out.contains("CpuBurner\\.burn");
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,event=cpu,nativemem,trace=test.instrument.CpuBurner.burn,jfr,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void manyEngines(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        HashMap<String, Long> eventsCount = new HashMap<>();
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                eventsCount.compute(event.getEventType().getName(), (key, old) -> old == null ? 1 : old + 1);
            }
        }
        assert eventsCount.get("jdk.MethodTrace") == 7 : eventsCount;
        assert eventsCount.get("profiler.Malloc") != null : eventsCount;
        assert eventsCount.get("jdk.ExecutionSample") != null : eventsCount;
    }

    @Test(
        mainClass = CpuBurnerManyTargets.class,
        agentArgs = "start,trace=test.instrument.CpuBurnerManyTargets.burn1:50ms,trace=test.instrument.CpuBurner.burn:10ms,total,threads,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void manyTraceTargets(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert out.samples("\\[thread1 .*;test\\/instrument\\/CpuBurnerManyTargets\\.burn1 ") >= Duration.ofMillis(50).toNanos();
        assert out.samples("\\[thread1 .*;test\\/instrument\\/CpuBurnerManyTargets\\.burn1;test\\/instrument\\/CpuBurner\\.burn ") >= Duration.ofMillis(50).toNanos();
        assert out.samples("\\[thread2 .*;test\\/instrument\\/CpuBurnerManyTargets\\.burn2;test\\/instrument\\/CpuBurner\\.burn ") >= Duration.ofMillis(10).toNanos();
    }

    @Test(
        mainClass = CpuBurnerManyTargets.class,
        agentArgs = "start,trace=test.instrument.CpuBurnerManyTargets.burn1:50ms,trace=test.instrument.CpuBurnerManyTargets.burn2:10ms,total,threads,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void manyTraceTargetsSameClass(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert out.samples("\\[thread1 .*;test\\/instrument\\/CpuBurnerManyTargets\\.burn1 ") >= Duration.ofMillis(50).toNanos();
        assert out.samples("\\[thread2 .*;test\\/instrument\\/CpuBurnerManyTargets\\.burn2 ") >= Duration.ofMillis(10).toNanos();
    }

    @Test(
        mainClass = CpuBurnerManyTargets.class,
        agentArgs = "start,trace=test.instrument.CpuBurnerManyTargets.burn1:50ms,trace=test.instrument.CpuBurnerManyTargets.burn2:1s,total,threads,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    // Tests many targets in the same class, and one target is never sampled
    public void manyTraceTargetsSameClassOneOut(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert out.samples("\\[thread1 .*;test\\/instrument\\/CpuBurnerManyTargets\\.burn1 ") >= Duration.ofMillis(50).toNanos();
        assert !out.contains("CpuBurnerManyTargets\\.burn2");
    }

    @Test(
        mainClass = CpuBurnerManyTargets.class,
        agentArgs = "start,event=cpu,nativemem,trace=test.instrument.CpuBurnerManyTargets.burn1,trace=test.instrument.CpuBurner.burn,jfr,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void manyEnginesManyTraceTargets(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        HashMap<String, Long> eventsCount = new HashMap<>();
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                eventsCount.compute(event.getEventType().getName(), (key, old) -> old == null ? 1 : old + 1);
            }
        }
        assert eventsCount.get("jdk.MethodTrace") == 3 : eventsCount;
        assert eventsCount.get("profiler.Malloc") != null : eventsCount;
        assert eventsCount.get("jdk.ExecutionSample") != null : eventsCount;
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,trace=test.instrument.CpuBurner.burn:100ms,jfr,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void latencyJfr(TestProcess p) throws Exception {
        p.waitForExit();
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        boolean found = false;
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                if (event.getEventType().getName().equals("jdk.MethodTrace")) {
                    found = true;
                    String repr = event.toString();
                    // TODO: Fix this test when 'method' is filled properly
                    assert repr.contains("method = N/A") : repr;
                }
            }
        }
        assert found : "Could not find any jdk.MethodTrace events";
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,trace=*.*:100ms,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    // Smoke test: if any validation failure happens Instrument::BytecodeRewriter has a bug
    public void latencyAll(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert out.samples("\\[thread1 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$0;test\\/instrument\\/CpuBurner\\.burn") == 1;
        assert out.samples("\\[thread2 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$1;test\\/instrument\\/CpuBurner\\.burn") == 2;
        assert !out.contains("\\[thread3.*");
        assert !out.contains("\\[thread4.*");
    }

    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,trace=test.instrument.CpuBurner.burn:100ms,total,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void latencyDuration(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        assert out.samples("\\[thread1 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$0;test\\/instrument\\/CpuBurner\\.burn") >= Duration.ofMillis(500).toNanos();
        assert out.samples("\\[thread2 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$1;test\\/instrument\\/CpuBurner\\.burn") >= Duration.ofMillis(300+150).toNanos();
        assert !out.contains("\\[thread3.*");
        assert !out.contains("\\[thread4.*");
    }

    @Test(
        mainClass = Recursive.class,
        agentArgs = "start,trace=test.instrument.Recursive.recursive:600ms,total,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void recursive(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        // recursive(i) = \sum_{j=i}^5 200*(MAX_RECURSION-j) ms
        // MAX_RECURSION = 3
        
        // recursive(3) is filtered out
        Duration duration = Duration.ZERO;
        assert !out.contains(MAIN_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + " ");
        
        // recursive(2) is filtered out
        duration = duration.plus(Duration.ofMillis(200));
        assert !out.contains(MAIN_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + " ");

        // recursive(1)
        duration = duration.plus(Duration.ofMillis(400));
        assert out.samples(MAIN_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + " ") >= duration.toNanos();

        // recursive(0)
        duration = duration.plus(Duration.ofMillis(600));
        assert out.samples(MAIN_METHOD_SEGMENT + RECURSIVE_METHOD_SEGMENT + " ") >= duration.toNanos();
    }

    @Test(
        mainClass = MethodTracingStop.class,
        jvmArgs   = "-Djava.library.path=build/lib -Xverify:all -XX:+IgnoreUnrecognizedVMOptions --enable-native-access=ALL-UNNAMED",
        output    = true,
        error     = true
    )
    public void stop(TestProcess p) throws Exception {
        p.waitForExit();
        assertNoVerificationErrors(p);
    }

    private static void assertNoVerificationErrors(TestProcess p) throws IOException {
        Output stdout = p.readFile(TestProcess.STDOUT);
        assert !stdout.contains("\\[ERROR\\]") && !stdout.contains("SIGSEGV") : stdout;

        Output stderr = p.readFile(TestProcess.STDERR);
        assert stderr.toString().isEmpty() : stderr;
    }

    // Check all calls are recorded when tracing/instrumenting test.instrument.CpuBurner.burn while running CpuBurner.main (threads enabled)
    private static void assertAllCallsRecorded(Output out) {
        assert out.samples("\\[thread1 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$0;test\\/instrument\\/CpuBurner\\.burn") == 2;
        assert out.samples("\\[thread2 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$1;test\\/instrument\\/CpuBurner\\.burn") == 3;
        assert out.samples("\\[thread3 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$2;test\\/instrument\\/CpuBurner\\.burn") == 1;
        assert out.samples("\\[thread4 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$3;test\\/instrument\\/CpuBurner\\.burn") == 1;
    }

}
