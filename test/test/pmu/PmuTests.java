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

    @Test(mainClass = Dictionary.class, os = Os.LINUX)
    public void cycles(TestProcess p) throws Exception {
        Double ratio16K;
        Double ratio8M;
        try {
            p.profile("-e cycles -d 3 -o collapsed -f %f");
            Output out = p.readFile("%f");
            ratio16K = out.ratio("test/pmu/Dictionary.test16K");
            ratio8M = out.ratio("test/pmu/Dictionary.test8M");
            Assert.isGreater(ratio16K, 0.4);
            Assert.isGreater(ratio8M, 0.4);
        } catch (Exception e) {
            System.out.println("Exception caught in PmuTests.cycles: " + e);
            System.out.println("Value of 16K ratio: " + String.valueOf(ratio16K) + " , Value of 8M Ratio: " + String.valueOf(ratio8M));
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
}
