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

import static one.profiler.test.TestConstants.PROFILE_ERROR_FIELD;

// Tests require perfevents to be enabled to pass
public class PmuTests {

    @Test(mainClass = Dictionary.class, os = {Os.LINUX})
    public void cycles(TestProcess p) throws Exception {
        try {
            p.profile("-e cycles -d 3 -o collapsed -f %f");
            Output out = p.readFile("%f");
            Assert.isGreater(out.ratio("test/pmu/Dictionary.test128K"), 0.4);
            Assert.isGreater(out.ratio("test/pmu/Dictionary.test8M"), 0.4);
        } catch (Exception e) {
            if (!p.readFile(PROFILE_ERROR_FIELD).contains("Perf events unavailable")) {
                throw e;
            }
        }
    }

    @Test(mainClass = Dictionary.class, os = {Os.LINUX}, arch = {Arch.X64, Arch.X86})
    public void cacheMisses(TestProcess p) throws Exception {
        try {
            p.profile("-e cache-misses -d 3 -o collapsed -f %f");

            Output out = p.readFile("%f");
            Assert.isLess(out.ratio("test/pmu/Dictionary.test128K"), 0.2);
            Assert.isGreater(out.ratio("test/pmu/Dictionary.test8M"), 0.8);
        } catch (Exception e) {
            if (!p.readFile(PROFILE_ERROR_FIELD).contains("Perf events unavailable")) {
                throw e;
            }
        }
    }

    @Test(mainClass = Dictionary.class, os = {Os.MACOS})
    public void pmuIncompatible(TestProcess p) throws Exception {
        try {
            p.profile("-e cache-misses -d 3 -o collapsed -f %f");
            throw new AssertionError("Somehow accessed PerfEvents on macOS???");
        } catch (IOException e) {
            Assert.contains(p.readFile(PROFILE_ERROR_FIELD), "PerfEvents are not supported on this platform");
        }
    }
}
