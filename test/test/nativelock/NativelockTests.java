/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativelock;

import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class NativelockTests {

    @Test(mainClass = CallsMutexLock.class)
    public void canAsprofTraceMutexContention(TestProcess p) throws Exception {
        Output out = p.profile("-e nativelock -d 3 -o collapsed");
        assert out.samples("pthread_mutex_lock_hook") > 0 : "No pthread_mutex_lock_hook samples captured";
    }

    @Test(mainClass = CallsRdLock.class)
    public void canAsprofTraceRdLockContention(TestProcess p) throws Exception {
        Output out = p.profile("-e nativelock -d 3 -o collapsed");
        assert out.samples("pthread_rwlock_rdlock_hook") > 0 : "No pthread_rwlock_rdlock_hook samples captured";
    }

    @Test(mainClass = CallsWrLock.class)
    public void canAsprofTraceWrLockContention(TestProcess p) throws Exception {
        Output out = p.profile("-e nativelock -d 3 -o collapsed");
        assert out.samples("pthread_rwlock_wrlock_hook") > 0 : "No pthread_rwlock_wrlock_hook samples captured";
    }

    @Test(mainClass = CallsMutexLock.class, agentArgs = "start,nativelock,collapsed,file=%f", args = "once")
    public void canAgentTraceMutexContention(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert out.samples("pthread_mutex_lock_hook") > 0 : "No pthread_mutex_lock_hook samples captured in agent mode";
    }

    @Test(mainClass = CallsRdLock.class, agentArgs = "start,nativelock,collapsed,file=%f", args = "once")
    public void canAgentTraceRdLockContention(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert out.samples("pthread_rwlock_rdlock_hook") > 0 : "No pthread_rwlock_rdlock_hook samples captured in agent mode";
    }

    @Test(mainClass = CallsWrLock.class, agentArgs = "start,nativelock,collapsed,file=%f", args = "once")
    public void canAgentTraceWrLockContention(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert out.samples("pthread_rwlock_wrlock_hook") > 0 : "No pthread_rwlock_wrlock_hook samples captured in agent mode";
    }

    @Test(mainClass = CallsMutexLock.class, os = Os.LINUX, args = "once", env = { "LD_PRELOAD=%lib", "ASPROF_COMMAND=start,nativelock,file=%f.jfr" })
    public void ldpreloadMutex(TestProcess p) throws Exception {
        p.waitForExit();
        Output out = Output.convertJfrToCollapsed(p.getFilePath("%f"), "--nativelock");
        assert out.samples("pthread_mutex_lock_hook") > 0 : "No pthread_mutex_lock_hook samples captured with LD_PRELOAD";
    }

    @Test(mainClass = CallsRdLock.class, os = Os.LINUX, args = "once", env = { "LD_PRELOAD=%lib", "ASPROF_COMMAND=start,nativelock,file=%f.jfr" })
    public void ldpreloadRdLock(TestProcess p) throws Exception {
        p.waitForExit();
        Output out = Output.convertJfrToCollapsed(p.getFilePath("%f"), "--nativelock");
        assert out.samples("pthread_rwlock_rdlock_hook") > 0 : "No pthread_rwlock_rdlock_hook samples captured with LD_PRELOAD";
    }

    @Test(mainClass = CallsWrLock.class, os = Os.LINUX, args = "once", env = { "LD_PRELOAD=%lib", "ASPROF_COMMAND=start,nativelock,file=%f.jfr" })
    public void ldpreloadWrLock(TestProcess p) throws Exception {
        p.waitForExit();
        Output out = Output.convertJfrToCollapsed(p.getFilePath("%f"), "--nativelock");
        assert out.samples("pthread_rwlock_wrlock_hook") > 0 : "No pthread_rwlock_wrlock_hook samples captured with LD_PRELOAD";
    }
}