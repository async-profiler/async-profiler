/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.instrument;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class InstrumentTests {

    @Test(mainClass = JavaProperties.class, agentArgs = "start,event=java.util.Properties.getProperty,collapsed,file=%profile", nameSuffix = "default")
    @Test(mainClass = JavaProperties.class, agentArgs = "start,event=java.util.Properties.getProperty,collapsed,cstack=vm,file=%profile", nameSuffix = "VM")
    public void instrument(TestProcess p) throws Exception {
        Output output = p.waitForExit("%profile");

        output.stream().forEach(line -> {
            assert line.matches(".*java/util/Properties.getProperty [0-9]+") : line;
        });
    }

    @Test(mainClass = JavaProperties.class, agentArgs = "start,event=java.util.Properties.getProperty,collapsed,cstack=vmx,file=%profile")
    public void instrumentVMX(TestProcess p) throws Exception {
        Output output = p.waitForExit("%profile");

        output.stream().forEach(line -> {
            assert line.matches(".*java/util/Properties.getProperty;" +
                    ".+ [0-9]+") : line; // Multiple internal profiler frames will be present in VMX
        });
    }
}
