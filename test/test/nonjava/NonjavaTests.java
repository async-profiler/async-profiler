/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nonjava;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class NonjavaTests {

    // jvm is loaded before the profiling session is started
    @Test(sh = "%testbin/non_java_app 1 %profile.collapsed", output = true)
    public void jvmFirst(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%profile");
        assert out.contains("cpuHeavyTask");
    }

    // jvm is loaded after the profiling session is started
    @Test(sh = "%testbin/non_java_app 2 %profile.collapsed", output = true)
    public void profilerFirst(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%profile");
        assert !out.contains("cpuHeavyTask");
    }

    // jvm is loaded between two profiling sessions
    @Test(sh = "%testbin/non_java_app 3 %noJvmProfile.collapsed %jvmProfile.collapsed", output = true)
    public void jvmInBetween(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%noJvmProfile");
        assert out.contains("nativeBurnCpu");
        assert !out.contains("cpuHeavyTask");

        out = p.readFile("%jvmProfile");
        assert out.contains("nativeBurnCpu");
        assert out.contains("cpuHeavyTask");
    }

    // jvm is loaded before the profiling session is started on a different thread
    @Test(sh = "%testbin/non_java_app 4 %profile.collapsed", output = true)
    public void jvmDifferentThread(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%profile");
        assert out.contains("cpuHeavyTask");
    }

    // Profile is dumped on a different native thread from the one that started the JVM/profiler
    @Test(sh = "%testbin/non_java_app 5 %profile.collapsed", output = true)
    public void dumpDifferentThread(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%profile");
        assert out.contains("cpuHeavyTask");
    }

    // Profile is re-started on a different native thread from the one that started the JVM/profiler
    @Test(sh = "%testbin/non_java_app 6 %profile.collapsed", output = true)
    public void startDifferentThread(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = p.readFile("%profile");
        assert out.contains("cpuHeavyTask");
    }

    // Profile is re-started on a different native thread from the one that started the JVM/profiler,
    // and profiler is stopped on a 3rd different native thread
    @Test(sh = "%testbin/non_java_app 7 %profile.jfr", output = true)
    public void startStopDifferentThreadsJfr(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output out = Output.convertJfrToCollapsed(p.getFilePath("%profile"));
        assert out.contains("cpuHeavyTask");
    }
}
