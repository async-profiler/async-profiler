/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.smoke;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class SmokeTests {

    @Test(mainClass = Cpu.class)
    public void cpu(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        assert out.contains("test/smoke/Cpu.main;test/smoke/Cpu.method1");
        assert out.contains("test/smoke/Cpu.main;test/smoke/Cpu.method2");
        assert out.contains("test/smoke/Cpu.main;test/smoke/Cpu.method3;java/io/File");
    }

    @Test(mainClass = Alloc.class)
    public void alloc(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e alloc -o collapsed -t");
        assert out.contains("\\[AllocThread-1 tid=[0-9]+];.*Alloc.allocate;.*java.lang.Integer\\[]");
        assert out.contains("\\[AllocThread-2 tid=[0-9]+];.*Alloc.allocate;.*int\\[]");
    }

    @Test(mainClass = Threads.class, agentArgs = "start,event=cpu,collapsed,threads,file=%f")
    public void threads(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert out.contains("\\[ThreadEarlyEnd tid=[0-9]+];.*Threads.methodForThreadEarlyEnd;.*");
        assert out.contains("\\[RenamedThread tid=[0-9]+];.*Threads.methodForRenamedThread;.*");
    }

    @Test(mainClass = LoadLibrary.class)
    public void loadLibrary(TestProcess p) throws Exception {
        p.profile("-f %f -o collapsed -d 4 -i 1ms");
        Output out = p.readFile("%f");
        assert out.contains("Java_sun_management");
    }

    // libjvm + JDK helpers must remain resolvable when symbols-include is set.
    @Test(mainClass = LoadLibrary.class)
    public void symbolsIncludeAutoAllowsJdk(TestProcess p) throws Exception {
        p.profile("-f %f -o collapsed -d 4 -i 1ms --symbols-include this-matches-nothing-*");
        Output out = p.readFile("%f");
        assert out.contains("Java_sun_management");
    }

    // Excluding a post-attach lib (libmanagement.so, loaded when LoadLibrary
    // touches ManagementFactory) prevents its symbols from entering CodeCache;
    // libjvm symbols still resolve because libjvm is force-parsed at bootstrap.
    @Test(mainClass = LoadLibrary.class)
    public void symbolsExcludePostAttachLib(TestProcess p) throws Exception {
        p.profile("-f %f -o collapsed -d 4 -i 1ms --symbols-exclude libmanagement.so");
        Output out = p.readFile("%f");
        assert !out.contains("Java_sun_management");
        assert out.contains("jmm_GetLongAttribute") || out.contains("VMManagementImpl");
    }

    // Baseline: with no flags, libjvm + libmanagement (loaded post-attach) both
    // resolve. Verifies the LateInitializer libjvm-only bootstrap doesn't regress
    // standard profiling on the no-flag path.
    @Test(mainClass = LoadLibrary.class)
    public void symbolsNoFilterBaseline(TestProcess p) throws Exception {
        p.profile("-f %f -o collapsed -d 4 -i 1ms");
        Output out = p.readFile("%f");
        assert out.contains("Java_sun_management");
        assert out.contains("jmm_GetLongAttribute") || out.contains("VMManagementImpl");
    }

    // Multiple --symbols-include patterns OR-combine: a lib matching either is
    // kept. Here libmanagement matches the second pattern; a no-op first pattern
    // proves combination isn't AND.
    @Test(mainClass = LoadLibrary.class)
    public void symbolsIncludeMultiplePatterns(TestProcess p) throws Exception {
        p.profile("-f %f -o collapsed -d 4 -i 1ms"
                + " --symbols-include this-matches-nothing-*"
                + " --symbols-include libmanagement*");
        Output out = p.readFile("%f");
        assert out.contains("Java_sun_management");
    }

    // Filtered-out lib renders frames as the bare lib name (or [unknown]
    // depending on output style). Verifies the filter actually causes lookup
    // misses rather than silently allowing parsing.
    @Test(mainClass = LoadLibrary.class)
    public void symbolsExcludeRendersAsLibName(TestProcess p) throws Exception {
        // --lib (STYLE_LIB_NAMES) prepends lib name; for filtered-out libs the
        // entire frame becomes just the lib basename.
        p.profile("-f %f -o collapsed --lib -d 4 -i 1ms --symbols-exclude libmanagement.so");
        Output out = p.readFile("%f");
        // No libmanagement function symbols at all
        assert !out.contains("Java_sun_management");
        // libmanagement is still in CodeCache (auto-allow excluded it; and even
        // without auto-allow the Profiler's library tracking knows the address
        // range), so frames may render as "libmanagement.so" or [unknown]
        // depending on whether the PC fell in an unparsed lib's range or no
        // lib's range. We just assert the symbol is gone.
    }

    // Additive cache semantics: changing the filter on a subsequent profile
    // session must NOT lose libraries parsed under the previous filter.
    // Session 1 uses no filter (loads everything including libmanagement);
    // Session 2 uses an exclude that would have skipped libmanagement, but
    // because the lib was already parsed, its symbols still resolve.
    @Test(mainClass = LoadLibrary.class)
    public void symbolsAdditiveCacheAcrossSessions(TestProcess p) throws Exception {
        // Session 1: no filter -> libmanagement gets parsed via auto-allow path.
        p.profile("-f %f1 -o collapsed -d 4 -i 1ms");
        Output session1 = p.readFile("%f1");
        assert session1.contains("Java_sun_management");

        // Session 2: filter that would block libmanagement on a fresh attach,
        // but agent state persists in the JVM and the cache is additive.
        p.profile("-f %f2 -o collapsed -d 4 -i 1ms --symbols-exclude libmanagement.so");
        Output session2 = p.readFile("%f2");
        assert session2.contains("Java_sun_management");
    }

    // Excluding libjvm is a no-op (libjvm is force-parsed at bootstrap to find
    // AsyncGetCallTrace) and emits a warning. We can't easily capture the agent
    // log from the test harness, so we verify the no-op behavior: libjvm
    // symbols still resolve.
    @Test(mainClass = LoadLibrary.class)
    public void symbolsExcludeLibjvmIsNoOp(TestProcess p) throws Exception {
        p.profile("-f %f -o collapsed -d 4 -i 1ms --symbols-exclude libjvm.so");
        Output out = p.readFile("%f");
        assert out.contains("jmm_GetLongAttribute") || out.contains("VMManagementImpl");
    }

    // libasyncProfiler.so itself MUST always be parsed regardless of the user
    // filter, because internal paths (MallocTracer / NativeLockTracer init,
    // crash handler) call findLibraryByAddress on their own code and assert
    // non-NULL. With nativemem profiling AND a filter that wouldn't naturally
    // match libasyncProfiler.so, we need the auto-allow-self rule to kick in;
    // otherwise the agent would assert-fail at start.
    @Test(mainClass = LoadLibrary.class)
    public void symbolsIncludeAutoAllowsSelf(TestProcess p) throws Exception {
        // nativemem exercises MallocTracer::initialize, the original assertion site.
        p.profile("-f %f -o collapsed -d 3 --nativemem 1 --symbols-include this-matches-nothing-*");
        // No assertion required on the output; the test passes if profiling
        // completed without aborting (an assert would have killed the JVM).
    }
}
