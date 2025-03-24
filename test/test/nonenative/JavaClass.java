/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

import java.lang.Math;

public class JavaClass {
    public void cpuHeavyTask() {
        for (int i = 0; i < 100000; i++) {
            Math.sqrt(Math.random());
            Math.pow(Math.random(), Math.random());
        }
    }
}
