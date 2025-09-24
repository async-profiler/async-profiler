/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

import java.util.ArrayList;
import java.util.List;

public class MemoryIntensiveApp {

    public static void main(String[] args) throws Exception {
        List<byte[]> memory = new ArrayList<>();
        for (int i = 0; i < 100; i++) {
            memory.add(new byte[1024 * 1024]); // 1MB each
            Thread.sleep(100);
        }

        Thread.sleep(5000);
    }
}
