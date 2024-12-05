/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.cstack;

import one.profiler.test.Jvm;
import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class CstackTests {
    private static final String PROFILE_COMMAND = "-e cpu -i 10ms -d 1 -o collapsed -a ";

    @Test(mainClass = LongInitializer.class)
    public void asyncGetCallTrace(TestProcess p) throws Exception {
        Output out = p.profile(PROFILE_COMMAND + "--cstack no");
        assert !out.contains(";readBytes");
        assert out.contains("LongInitializer.main_\\[j]");

        out = p.profile(PROFILE_COMMAND + "--cstack fp");
        assert out.contains(";readBytes");
        assert out.contains("LongInitializer.main_\\[j]");
    }

    @Test(mainClass = LongInitializer.class, jvm = Jvm.HOTSPOT, os = Os.LINUX)
    public void vmStructs(TestProcess p) throws Exception {
        Output out = p.profile(PROFILE_COMMAND + "--cstack vm");
        assert out.contains(";readBytes");
        assert out.contains("LongInitializer.main_\\[0]");
        assert !out.contains("InstanceKlass::initialize");
        assert !out.contains("call_stub");
        assert !out.contains("JavaMain");

        out = p.profile(PROFILE_COMMAND + "--cstack vmx");
        assert out.contains(";readBytes");
        assert out.contains("LongInitializer.main_\\[0]");
        assert out.contains("InstanceKlass::initialize");
        assert out.contains("call_stub");
        assert out.contains("JavaMain");
    }
}
