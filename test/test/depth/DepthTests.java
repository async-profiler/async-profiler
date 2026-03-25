/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.depth;

import one.profiler.test.Assert;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

import java.util.List;
import java.util.stream.Collectors;

public class DepthTests {

    private static long frameCount(String stack) {
        return stack.chars().filter(c -> c == ';').count() + 1;
    }

    @Test(mainClass = DeepRecursion.class)
    public void maxDepth(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu --all-user -d 2 -s -o collapsed -j 50");

        // Overall profile depth is exactly 50 frames
        long depth = out.stream().mapToLong(DepthTests::frameCount).max().getAsLong();
        Assert.isEqual(depth, 50);

        // At least some non-truncated stacks are smaller
        assert out.stream("^DeepRecursion.main").anyMatch(s -> frameCount(s) < 50);

        // Flame graph has exactly 51 levels (+1 for the root frame)
        out = p.profile("stop -o flamegraph");
        assert out.containsExact("Array(51)");

        out = p.profile("-e cpu --all-user -d 2 -s -o collapsed -j 50/20");

        // Non-truncated stacks can be anything between 1 and 50 frames
        List<String> full = out.stream("^DeepRecursion.main").collect(Collectors.toList());
        assert full.stream().allMatch(s -> frameCount(s) < 50);
        assert full.stream().anyMatch(s -> frameCount(s) < 20);

        // All truncated stacks start with [truncated] followed by exactly 20 frames
        List<String> truncated = out.stream("^(?!DeepRecursion.main)").collect(Collectors.toList());
        assert truncated.stream().allMatch(s -> s.startsWith("[truncated];"));
        assert truncated.stream().allMatch(s -> frameCount(s) == 21);
    }
}
