/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.comptask;

import one.profiler.test.*;

public class ComptaskTests {
    @Test(
        mainClass = Main.class,
        agentArgs = "start,features=comptask,interval=10ns,collapsed",
        jvmArgs = "-Xcomp",
        jvm = Jvm.HOTSPOT,
        output = true
    )
    public void testCompTask(TestProcess p) throws Exception {
        Output out = p.waitForExit(TestProcess.STDOUT);
        assert p.exitCode() == 0;
        assert out.contains("CompileBroker::invoke_compiler_on_method;test/comptask/Main.main");
    }
}
