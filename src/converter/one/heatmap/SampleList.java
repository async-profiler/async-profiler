/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;

public class SampleList {

    private static final int DEFAULT_SAMPLES_COUNT = 10_000_000;

    private final long blockDurationMs;

    // highest 32 bits for time block index, lowest 32 bits for stack id
    private long[] data = new long[DEFAULT_SAMPLES_COUNT];

    private long initialTime = 0;
    private int recordsCount = 0;

    public SampleList(long blockDurationMs) {
        this.blockDurationMs = blockDurationMs;
    }

    public void add(int stackId, long timeMs) {
        if (initialTime == 0) {
            initialTime = timeMs;
            data[recordsCount++] = stackId;
            return;
        }
        if (recordsCount >= data.length) {
            data = Arrays.copyOf(data, data.length * 3 / 2);
        }

        int currentTimeBlock = (int) ((timeMs - initialTime) / blockDurationMs);
        data[recordsCount++] = (long) currentTimeBlock << 32 | stackId;
    }

    public Result samples() {
        Arrays.sort(data, 0, recordsCount);

        int firstBlockId = (int) (data[0] >> 32);
        int lastBlockId = (int) (data[recordsCount - 1] >> 32);

        int blocksCount = lastBlockId - firstBlockId + 1;

        int[] blockSizes = new int[blocksCount];
        int[] stackIds = new int[recordsCount];

        int stackIdsPos = 0;
        int currentBlockIndex = 0;
        int currentBlockSize = 0;
        int currentBlockId = firstBlockId;

        outer:
        while (stackIdsPos < stackIds.length) {
            long currentData = data[stackIdsPos];
            int blockId = (int) (currentData >> 32);
            while (currentBlockId != blockId) {
                blockSizes[currentBlockIndex++] = currentBlockSize;
                currentBlockSize = 0;
                currentBlockId++;
                if (currentBlockId > lastBlockId) {
                    break outer;
                }
            }

            currentBlockSize++;
            stackIds[stackIdsPos++] = (int) (currentData & 0xFFFFFFFFL) - 1;
        }

        if (currentBlockId <= lastBlockId) {
            blockSizes[currentBlockIndex] = currentBlockSize;
        }

        return new Result(blockSizes, stackIds);
    }

    public int getRecordsCount() {
        return recordsCount;
    }

    public static class Result {
        public final int[] blockSizes;
        public final int[] stackIds;

        public Result(int[] blockSizes, int[] stackIds) {
            this.blockSizes = blockSizes;
            this.stackIds = stackIds;
        }
    }

}
