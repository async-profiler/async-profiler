/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.pmu;

import one.profiler.test.Arch;
import one.profiler.test.Output;

import java.io.IOException;

import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.Os;

public class PmuTests {
    private static final String GITHUB_ACTIONS_ENV_VAR = "GITHUB_ACTIONS";
    private static final String OS_ARCH_ENV_VAR = "os.arch";
    private static final String AARCH64 = "aarch64";
    private static final String ARM = "arm";

    @Test(mainClass = Dictionary.class, os = Os.LINUX)
    public void cycles(TestProcess p) throws Exception {
        try {
            // We are skipping the test in one case, for more details: https://github.com/actions/runner-images/issues/11689
            if (isOsARMBased() && isRunningOnGithubActions()) {
                System.out.println("Skipping the test PmuTests.cycles on ARM in GitHub Actions");
                return;
            }
            p.profile("-e cycles -d 3 -o collapsed -f %f");
            Output out = p.readFile("%f");
            Assert.isGreater(out.ratio("test/pmu/Dictionary.test16K"), 0.4);
            Assert.isGreater(out.ratio("test/pmu/Dictionary.test8M"), 0.4);
        } catch (Exception e) {
            if (!p.readFile(TestProcess.PROFERR).contains("Perf events unavailable")) {
                throw e;
            }
        }
    }

    @Test(mainClass = Dictionary.class, os = Os.LINUX, arch = {Arch.X64, Arch.X86})
    public void cacheMisses(TestProcess p) throws Exception {
        try {
            p.profile("-e cache-misses -d 3 -o collapsed -f %f");

            Output out = p.readFile("%f");
            Assert.isLess(out.ratio("test/pmu/Dictionary.test16K"), 0.2);
            Assert.isGreater(out.ratio("test/pmu/Dictionary.test8M"), 0.8);
        } catch (Exception e) {
            if (!p.readFile(TestProcess.PROFERR).contains("Perf events unavailable")) {
                throw e;
            }
        }
    }

    @Test(mainClass = Dictionary.class, os = Os.MACOS)
    public void pmuIncompatible(TestProcess p) throws Exception {
        try {
            p.profile("-e cache-misses -d 3 -o collapsed -f %f");
            throw new AssertionError("PerfEvents should succeed on Linux only");
        } catch (IOException e) {
            assert p.readFile(TestProcess.PROFERR).contains("PerfEvents are not supported on this platform");
        }
    }

    private boolean isOsARMBased() {
        String arch = System.getProperty(OS_ARCH_ENV_VAR);
        return arch.contains(AARCH64) || arch.contains(ARM);
    }

    private boolean isRunningOnGithubActions() {
        return System.getenv(GITHUB_ACTIONS_ENV_VAR) != null && System.getenv(GITHUB_ACTIONS_ENV_VAR).equals("true");
    }
}
