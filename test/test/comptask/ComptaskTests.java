/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.comptask;

import one.profiler.test.*;

public class ComptaskTests {
    @Test(
        mainClass = Main.class,
        agentArgs = "start,features=comptask,collapsed,interval=1ms,file=%f",
        jvmArgs = "-Xcomp",
        jvm = Jvm.HOTSPOT,
        // TODO: remove version filter after the fix for JDK-8367689 is released
        jvmVer = {8, 24}
    )
    public void testCompTask(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;
        assert out.contains(";Compiler::compile_method;(java|sun|jdk)/[^;]+[^;/]\\.[^;/]+;");
        assert out.contains(";C2Compiler::compile_method;(java|sun|jdk)/[^;]+[^;/]\\.[^;/]+;");
    }
}
