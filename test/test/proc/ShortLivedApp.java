/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

import java.util.ArrayList;
import java.util.List;

public class ShortLivedApp {

    private static final int SPAWN = 10;

    private static final String[] DD_CMD = {
        "timeout", "6",
        "dd",
        "if=/dev/zero",
        "of=/dev/null",
        "bs=1M",
        "status=none"
    };

    public static void main(String[] args) throws Exception {

        List<Process> procs = new ArrayList<>(SPAWN);

        for (int i = 0; i < SPAWN; i++) {
            procs.add(new ProcessBuilder(DD_CMD).start());
        }

        for (Process p : procs) {
            p.waitFor();
        }

        Thread.sleep(9000);
    }
}
