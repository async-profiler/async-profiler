/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;

public final class StackStorage {

    private static final int INITIAL_CAPACITY = 16 * 1024;

    private int size;

    // highest 32 bits for index, lowest 32 bits for hash
    private long[] meta;

    // ordered incrementally
    private int[][] values;

    public StackStorage() {
        this(INITIAL_CAPACITY);
    }

    public StackStorage(int initialCapacity) {
        meta = new long[initialCapacity * 2];
        values = new int[initialCapacity][];
    }

    public int[] get(int id) {
        return values[id - 1];
    }

    public int index(int[] input, int inputSize) {
        int mask = meta.length - 1;
        int targetHash = murmur(input, inputSize);
        int i = targetHash & mask;
        for (long currentMeta = meta[i]; currentMeta != 0; currentMeta = meta[i]) {
            if ((int) currentMeta == targetHash) {
                int index = (int) (currentMeta >>> 32);
                if (equals(input, inputSize, values[index])) {
                    return index + 1;
                }
            }
            i = (i + 1) & mask;
        }

        values[size] = Arrays.copyOf(input, inputSize);
        meta[i] = (long) size << 32 | (targetHash & 0xFFFFFFFFL);
        size++;

        if (size * 2 > values.length) {
            resize(values.length * 2);
        }

        return size;
    }

    public int[][] orderedTraces() {
        return Arrays.copyOf(values, size);
    }

    private void resize(int newCapacity) {
        long[] newMeta = new long[newCapacity * 2];
        int mask = newMeta.length - 1;

        for (long m : meta) {
            if (m != 0) {
                int hashCode = (int) m;
                for (int j = hashCode & mask; ; j = (j + 1) & mask) {
                    if (newMeta[j] == 0) {
                        newMeta[j] = m;
                        break;
                    }
                }
            }
        }

        meta = newMeta;
        values = Arrays.copyOf(values, newCapacity);
    }

    private boolean equals(int[] a, int size, int[] b) {
        if (b.length != size) return false;
        for (int i = 0; i < size; ++i) {
            if (a[i] != b[i]) return false;
        }
        return true;
    }

    private static int murmur(int[] data, int size) {
        int m = 0x5bd1e995;
        int h = 0x9747b28c ^ size;

        for (int i = 0; i < size; i++) {
            int k = data[i];
            k *= m;
            k ^= k >>> 24;
            k *= m;
            h *= m;
            h ^= k;
        }

        h ^= h >>> 13;
        h *= m;
        h ^= h >>> 15;

        return h;
    }
}
