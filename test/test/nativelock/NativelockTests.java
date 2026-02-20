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

    @Test(mainClass = AllNativeLocks.class)
    public void canAsprofTraceAllLockTypes(TestProcess p) throws Exception {
        Output out = p.profile("-e nativelock -d 3 -o collapsed");
        assert out.contains("pthread_mutex_lock_hook") : "No mutex samples captured";
        assert out.contains("pthread_rwlock_rdlock_hook") : "No rdlock samples captured";
        assert out.contains("pthread_rwlock_wrlock_hook") : "No wrlock samples captured";
    }

    @Test(mainClass = AllNativeLocks.class, agentArgs = "start,nativelock,collapsed,file=%f", args = "once")
    public void canAgentTraceAllLockTypes(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert out.contains("pthread_mutex_lock_hook") : "No mutex samples captured in agent mode";
        assert out.contains("pthread_rwlock_rdlock_hook") : "No rdlock samples captured in agent mode";
        assert out.contains("pthread_rwlock_wrlock_hook") : "No wrlock samples captured in agent mode";
    }

    @Test(mainClass = AllNativeLocks.class, os = Os.LINUX, args = "once", env = { "LD_PRELOAD=%lib", "ASPROF_COMMAND=start,nativelock,file=%f.jfr" })
    public void ldpreloadAllLockTypes(TestProcess p) throws Exception {
        p.waitForExit();
        Output out = Output.convertJfrToCollapsed(p.getFilePath("%f"), "--nativelock");
        assert out.contains("pthread_mutex_lock_hook") : "No mutex samples captured with LD_PRELOAD";
        assert out.contains("pthread_rwlock_rdlock_hook") : "No rdlock samples captured with LD_PRELOAD";
        assert out.contains("pthread_rwlock_wrlock_hook") : "No wrlock samples captured with LD_PRELOAD";
    }

    @Test(sh = "LD_PRELOAD=%lib ASPROF_COMMAND=start,nativelock,file=%f.jfr %testbin/native_lock_contention", os = Os.LINUX, runIsolated = true)
    public void nativeAllLockContention(TestProcess p) throws Exception {
        p.waitForExit();
        Output out = Output.convertJfrToCollapsed(p.getFilePath("%f"), "--nativelock");
        assert out.contains("pthread_mutex_lock_hook") : "No mutex samples captured in pure native test";
        assert out.contains("pthread_rwlock_rdlock_hook") : "No rdlock samples captured in pure native test";
        assert out.contains("pthread_rwlock_wrlock_hook") : "No wrlock samples captured in pure native test";
    }
}
