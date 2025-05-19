/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.comptask;

import one.profiler.test.*;

public class ComptaskTests {
    @Test(
        mainClass = Main.class,
        agentArgs = "start,features=comptask,collapsed,file=%f",
        jvmArgs = "-XX:+PrintCompilation -Xcomp",
        jvm = Jvm.HOTSPOT
    )
    public void testCompTask(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;
        System.err.println(out);
        assert out.contains("CompileBroker::invoke_compiler_on_method;test/comptask/Main.main");
    }
}
