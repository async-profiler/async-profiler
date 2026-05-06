/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.vmstructs;

import one.profiler.test.Assert;
import one.profiler.test.Jvm;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import test.alloc.Hello;
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

    @Test(mainClass = Hello.class, jvmArgs = "-Xcheck:jni", agentArgs = "start,event=alloc", output = true)
    public void checkJni(TestProcess p) throws Exception {
        p.waitForExit();
        Assert.isEqual(p.exitCode(), 0);
    }

    @Test(
        mainClass = DoBusyWork.class,
        agentArgs = "start,event=cpu,collapsed,interval=1ms,file=%f",
        jvmArgs = "-XX:+UnlockExperimentalVMOptions -XX:+HotCodeHeap -XX:+NMethodRelocation -XX:HotCodeIntervalSeconds=0 -XX:HotCodeSampleSeconds=1 -XX:HotCodeStablePercent=-1 -XX:HotCodeStartupDelaySeconds=0",
        jvm = Jvm.HOTSPOT,
        jvmVer = {27, Integer.MAX_VALUE},
        runIsolated = true
    )
    public void checkHotCodeHeap(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert out.contains("DoBusyWork\\.main");
        assert out.contains("DoBusyWork\\.doBusyWork");
        assert !out.contains("unknown_nmethod");
        assert !out.contains("no_Java_frame");
    }
}
