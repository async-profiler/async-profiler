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

    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=dwarf,collapsed,file=%profile", args = "sync", nameSuffix = "dwarf")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=fp,collapsed,file=%profile", args = "sync", nameSuffix = "fp")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=vm,collapsed,file=%profile", args = "sync", nameSuffix = "vm")
    public void syncLock(TestProcess p) throws Exception {
        Output out = p.waitForExit("%profile");
        assert p.exitCode() == 0;

        assert out.contains("test/lock/SimpleLockTest.syncLock;" +
                "java.lang.Class_\\[i\\] [0-9]+");
    }

    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=dwarf,collapsed,file=%profile", args = "sem", nameSuffix = "dwarf")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=fp,collapsed,file=%profile", args = "sem", nameSuffix = "fp")
    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=vm,collapsed,file=%profile", args = "sem", nameSuffix = "vm")
    public void semLock(TestProcess p) throws Exception {
        Output out = p.waitForExit("%profile");
        assert p.exitCode() == 0;

        assert out.contains("java/util/concurrent/locks/LockSupport.park;" +
                "(sun/misc/Unsafe.park;|jdk/internal/misc/Unsafe.park;)" +
                "java.util.concurrent.locks.ReentrantLock\\$NonfairSync_\\[i\\] [0-9]+");
    }

    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=vmx,collapsed,file=%profile", args = "sync")
    public void syncLockVMX(TestProcess p) throws Exception {
        Output out = p.waitForExit("%profile");
        assert p.exitCode() == 0;

        assert out.contains("test/lock/SimpleLockTest.syncLock;" +
                ".+;" + // VMX will have a number of native frames between java methods & top frame
                "java.lang.Class_\\[i\\] [0-9]+");
    }

    @Test(mainClass = SimpleLockTest.class, agentArgs = "start,event=lock,cstack=vmx,collapsed,file=%profile", args = "sem")
    public void semLockVMX(TestProcess p) throws Exception {
        Output out = p.waitForExit("%profile");
        assert p.exitCode() == 0;

        assert out.contains("java/util/concurrent/locks/LockSupport.park;" +
                "(sun/misc/Unsafe.park;|jdk/internal/misc/Unsafe.park;)" +
                ".+;" + // VMX will have a number of native frames between java methods & top frame
                "java.util.concurrent.locks.ReentrantLock\\$NonfairSync_\\[i\\] [0-9]+");
    }
}
