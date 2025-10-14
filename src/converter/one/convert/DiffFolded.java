/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintStream;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public final class DiffFolded {

    public static void process(String file1, String file2, String outputFile) throws IOException {
        try (PrintStream output = new PrintStream(outputFile)) {
            processFiles(file1, file2, output);
        }
    }

    private static void processFiles(String file1, String file2, PrintStream output) throws IOException {
        Map<String, Long> folded1 = readCollapsedFile(file1);
        Map<String, Long> folded2 = readCollapsedFile(file2);

        long total1 = folded1.values().stream().mapToLong(Long::longValue).sum();
        long total2 = folded2.values().stream().mapToLong(Long::longValue).sum();

        Set<String> allStacks = new HashSet<>();
        allStacks.addAll(folded1.keySet());
        allStacks.addAll(folded2.keySet());

        for (String stack : allStacks) {
            long count1 = folded1.getOrDefault(stack, 0L);
            long count2 = folded2.getOrDefault(stack, 0L);

            // Always apply count normalization for meaningful differential analysis
            if (total1 != total2 && total1 > 0) {
                count1 = (long) (count1 * total2 / total1);
            }

            // Only output stacks that exist in profile2 (count2 > 0)
            // This ensures the flame graph structure matches f2
            if (count2 > 0) {
                output.println(stack + " " + count1 + " " + count2);
            }
        }
    }

    private static Map<String, Long> readCollapsedFile(String filename) throws IOException {
        Map<String, Long> folded = new HashMap<>();

        try (BufferedReader br = new BufferedReader(new FileReader(filename))) {
            String line;
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty()) continue;

                int lastSpace = line.lastIndexOf(' ');
                if (lastSpace <= 0) continue;

                String stack = line.substring(0, lastSpace);
                long count = Long.parseLong(line.substring(lastSpace + 1));

                // Accumulate counts for the same stack
                folded.merge(stack, count, Long::sum);
            }
        }

        return folded;
    }
}
