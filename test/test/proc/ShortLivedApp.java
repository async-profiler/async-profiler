/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

import java.util.ArrayList;
import java.util.List;

public class ShortLivedApp {

    private static final String[] DD_CMD = {"timeout", "2", "dd", "if=/dev/zero", "of=/dev/null", "bs=1M", "status=none"};

    public static void main(String[] args) throws Exception {
        while (true) {
            for (int i = 0; i < 10; i++) {
                new ProcessBuilder(DD_CMD).start();
            }
            Thread.sleep(500);
        }
    }
}
