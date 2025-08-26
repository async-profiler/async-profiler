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

        assert out.samples("\\[thread1 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$0;test\\/instrument\\/CpuBurner\\.burn") == 2;
        assert out.samples("\\[thread2 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$1;test\\/instrument\\/CpuBurner\\.burn") == 3;
        assert out.samples("\\[thread3 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$2;test\\/instrument\\/CpuBurner\\.burn") == 1;
        assert out.samples("\\[thread4 .*;test\\/instrument\\/CpuBurner\\.lambda\\$main\\$3;test\\/instrument\\/CpuBurner\\.burn") == 1;
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
        assert out.contains("java\\/lang\\/String\\.<init> ");
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
        agentArgs = "start,threads,event=test.instrument.CpuBurner.burn,latency=100ms,collapsed,file=%f",
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
        agentArgs = "start,threads,event=test.instrument.CpuBurner.burn,latency=100ms,jfr,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
    public void latencyJfr(TestProcess p) throws Exception {
        p.waitForExit();
        assertNoVerificationErrors(p);
        assert p.exitCode() == 0;

        StringBuilder builder = new StringBuilder();
        try (RecordingFile recordingFile = new RecordingFile(p.getFile("%f").toPath())) {
            while (recordingFile.hasMoreEvents()) {
                RecordedEvent event = recordingFile.readEvent();
                builder.append(event);
            }
        }
        String parsedOut = builder.toString();
        assert parsedOut.contains("jdk.MethodTrace");
    }

    // Smoke test: if any validation failure happens Instrument::BytecodeRewriter has a bug
    @Test(
        mainClass = CpuBurner.class,
        agentArgs = "start,threads,event=*.*,latency=100ms,collapsed,file=%f",
        jvmArgs   = "-Xverify:all",
        output    = true,
        error     = true
    )
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
        agentArgs = "start,threads,event=test.instrument.CpuBurner.burn,total,latency=100ms,collapsed,file=%f",
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
        agentArgs = "start,event=test.instrument.Recursive.recursive,total,latency=600ms,collapsed,file=%f",
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
        assert !out.contains(String.format("%s%s%s%s%s ", MAIN_METHOD_SEGMENT, RECURSIVE_METHOD_SEGMENT, RECURSIVE_METHOD_SEGMENT, RECURSIVE_METHOD_SEGMENT, 
                RECURSIVE_METHOD_SEGMENT));
        
        // recursive(2) is filtered out
        duration = duration.plus(Duration.ofMillis(200));
        assert !out.contains(String.format("%s%s%s%s ", MAIN_METHOD_SEGMENT, RECURSIVE_METHOD_SEGMENT, RECURSIVE_METHOD_SEGMENT, RECURSIVE_METHOD_SEGMENT));

        // recursive(1)
        duration = duration.plus(Duration.ofMillis(400));
        assert out.samples(String.format("%s%s%s ", MAIN_METHOD_SEGMENT, RECURSIVE_METHOD_SEGMENT, RECURSIVE_METHOD_SEGMENT)) >= duration.toNanos();

        // recursive(0)
        duration = duration.plus(Duration.ofMillis(600));
        assert out.samples(String.format("%s%s ", MAIN_METHOD_SEGMENT, RECURSIVE_METHOD_SEGMENT)) >= duration.toNanos();
    }

    private static void assertNoVerificationErrors(TestProcess p) throws IOException {
        String stdout = new String(Files.readAllBytes(p.getFile(TestProcess.STDOUT).toPath()));
        assert !stdout.contains("[ERROR]") : stdout;

        String stderr = new String(Files.readAllBytes(p.getFile(TestProcess.STDERR).toPath()));
        assert stderr.isEmpty() : stderr;
    }

}
