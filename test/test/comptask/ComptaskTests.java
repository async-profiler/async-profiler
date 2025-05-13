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
        // -Xbatch                            : Compilation happens synchronously as soon as the method becomes eligible for compilation
        // -XX:CompileThreshold=1             : Methods are compiled when they are first called
        // -XX:-TieredCompilation             : Disable tiered compilation, otherwise CompileThreshold is ignored
        // -XX:CompileCommand=compileonly,... : Select only specific methods to be compiled
        jvmArgs = "-Xbatch -XX:CompileThreshold=1 -XX:-TieredCompilation -XX:CompileCommand=compileonly,test.comptask.Main::main",
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
