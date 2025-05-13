/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.comptask;

import one.profiler.test.*;

public class ComptaskTests {
    @Test(
        mainClass = Main.class,
        agentArgs = "start,features=comptask,event=Compile::Init,collapsed",
        jvmArgs = "-Xcomp",
        jvm = Jvm.HOTSPOT,
        // No perf_events on MacOS
        os = Os.LINUX,
        output = true
    )
    public void testCompTask(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;
        assert out.contains("test/comptask/Main.main;C2Compiler::compile_method");
    }
}
