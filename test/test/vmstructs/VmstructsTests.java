/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.vmstructs;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import test.smoke.Alloc;

public class VmstructsTests {

    @Test(mainClass = Alloc.class, jvmArgs = "-XX:StartFlightRecording=filename=%f.jfr,settings=profile")
    public void jnienv(TestProcess p) throws Exception {
        Output out = p.profile("-d 2 --wall 50ms -F jnienv -o collapsed");
        assert out.contains("Alloc.allocate");
    }

    @Test(mainClass = Alloc.class, jvmArgs = "-XX:StartFlightRecording=filename=%f.jfr,settings=profile",
            agentArgs = "start,wall=50ms,features=jnienv")
    public void jnienvAgent(TestProcess p) throws Exception {
        Thread.sleep(2000);
        Output out = p.profile("stop -o collapsed");
        assert out.contains("Alloc.allocate");
    }
}
