/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

import java.util.ArrayList;
import java.util.List;

public class RandomBlockRetainer {
    public static void main(String[] args) throws Exception {
        double keepChance = 0.5;
        if (args.length > 0) {
            try {
                keepChance = Double.parseDouble(args[0]);
            } catch (NumberFormatException e) {
                System.err.println("Invalid keepChance value. Using default value of 0.5.");
            }
        }

        // Set up a list to hold large objects and keep them in memory.
        List<byte[]> rooter = new ArrayList<>();

        final int TOTAL_BLOCKS = 500; // Has to be less than LiveRefs::MAX_REFS for testing purposes.
        final int BLOCK_SIZE = 100 * 1000;

        for (int i = 0; i < TOTAL_BLOCKS; i++) {
            byte[] block = i % 2 == 0 ? alloc1(BLOCK_SIZE) : alloc2(BLOCK_SIZE);

            if (Math.random() <= keepChance) {
                // Keep a reference to prevent the block from being garbage collected
                rooter.add(block);
            }
        }

        System.gc();
    }

    private static byte[] alloc1(int size) {
        return new byte[size];
    }

    private static byte[] alloc2(int size) {
        return new byte[size];
    }
}
