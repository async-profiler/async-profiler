/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import one.profiler.AsyncProfiler;
import one.profiler.Events;

public class PrintProfilerVersion {
    public static void main(String[] args) throws Exception {
        System.out.println(AsyncProfiler.getInstance().getVersion());
    }
}
