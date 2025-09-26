/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

import java.util.ArrayList;
import java.util.List;

public class ManyProcessApp {

    public static void main(String[] args) throws Exception {
        List<Process> procs = new ArrayList<>();

        for (int i = 0; i < 5000; i++) {
            ProcessBuilder pb = new ProcessBuilder("sleep", "10");
            procs.add(pb.start());
        }

        for (Process p : procs) {
            p.waitFor();
        }

        Thread.sleep(3000);
    }
}
