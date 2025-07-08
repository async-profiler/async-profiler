/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class LockTests {

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true)
    public void datagramSocketLock(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed --cstack dwarf");
        assert out.ratio("(PlatformEvent::.ark|PlatformEvent::.npark)") > 0.1
                || ((out.ratio("ReentrantLock.lock") + out.ratio("ReentrantLock.unlock")) > 0.1 && out.contains("Unsafe_.ark"));
        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.contains("sun/nio/ch/DatagramChannelImpl.send");
    }

    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=dwarf,collapsed,file=%f", args = "sync", nameSuffix = "dwarf")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=fp,collapsed,file=%f", args = "sync", nameSuffix = "fp")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=vm,collapsed,file=%f", args = "sync", nameSuffix = "vm")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=vmx,collapsed,file=%f", args = "sync", nameSuffix = "vmx")
    public void syncLock(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;

        assert out.contains("test/lock/SimpleLockTest.syncLock;" +
                "java.lang.Class_\\[i\\]");
    }

    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=dwarf,collapsed,file=%f", args = "sem", nameSuffix = "dwarf")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=fp,collapsed,file=%f", args = "sem", nameSuffix = "fp")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=vm,collapsed,file=%f", args = "sem", nameSuffix = "vm")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=vmx,collapsed,file=%f", args = "sem", nameSuffix = "vmx")
    public void semLock(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;

        assert out.contains("java/util/concurrent/locks/LockSupport.park;" +
                "(sun/misc/Unsafe.park;|jdk/internal/misc/Unsafe.park;)" +
                "java.util.concurrent.Semaphore\\$NonfairSync_\\[i\\]");
    }
}
