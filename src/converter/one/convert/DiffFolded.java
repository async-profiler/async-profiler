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

    public static DifferentialResult process(ProfileData profile1, ProfileData profile2) {
        Map<String, String> stackData = new HashMap<>();
        Map<String, Long> functionDeltas = new HashMap<>();
        long maxDelta = 1;
        
        Map<String, Long> folded1 = profile1.collapsedData;
        Map<String, Long> folded2 = profile2.collapsedData;
        
        long total1 = folded1.values().stream().mapToLong(Long::longValue).sum();
        long total2 = folded2.values().stream().mapToLong(Long::longValue).sum();

        boolean shouldNormalize = shouldNormalizeCounts(profile1, profile2, total1, total2);

        Set<String> allStacks = new HashSet<>(folded1.keySet());
        allStacks.addAll(folded2.keySet());

        for (String stack : allStacks) {
            long count1 = folded1.getOrDefault(stack, 0L);
            long count2 = folded2.getOrDefault(stack, 0L);

            // Apply normalization only when appropriate
            if (shouldNormalize && total1 != total2 && total1 > 0) {
                count1 = (long) (count1 * total2 / total1);
            }

            // Only output stacks that exist in profile2 (count2 > 0)
            // This ensures the flame graph structure matches f2
            if (count2 > 0) {
                stackData.put(stack, count1 + " " + count2);
                long delta = count2 - count1;
                
                // Add delta to each function in the stack
                for (int from = 0, to; from < stack.length(); from = to + 1) {
                    if ((to = stack.indexOf(';', from)) < 0) to = stack.length();
                    String funcName = stack.substring(from, to);
                    
                    if (funcName.endsWith("_[0]") || funcName.endsWith("_[j]") || 
                        funcName.endsWith("_[i]") || funcName.endsWith("_[k]") || 
                        funcName.endsWith("_[1]")) {
                        funcName = funcName.substring(0, funcName.length() - 4);
                    }
                    
                    functionDeltas.merge(funcName, delta, Long::sum);
                }
            }
        }
        
        for (Long functionDelta : functionDeltas.values()) {
            maxDelta = Math.max(maxDelta, Math.abs(functionDelta));
        }
        
        return new DifferentialResult(stackData, functionDeltas, maxDelta);
    }

    private static boolean shouldNormalizeCounts(ProfileData profile1, ProfileData profile2, long total1, long total2) {
        if (profile1.durationSeconds == null || profile2.durationSeconds == null) return true;
        
        long roundedDuration1 = Math.round(profile1.durationSeconds);
        long roundedDuration2 = Math.round(profile2.durationSeconds);
        
        if (roundedDuration1 == roundedDuration2) return false;
        
        return true;
    }

    public static Map<String, Long> readCollapsedFile(String filename) throws IOException {
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
