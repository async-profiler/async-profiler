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

    public int index(int[] prototype, int stackSize, int[] prefix, int[] suffix) {
        if (prefix == null) prefix = new int[0];
        if (suffix == null) suffix = new int[0];

        int mask = meta.length - 1;
        int targetHash = murmur(prototype, stackSize, prefix, suffix);
        int i = targetHash & mask;
        for (long currentMeta = meta[i]; currentMeta != 0; currentMeta = meta[i]) {
            if ((int) currentMeta == targetHash) {
                int index = (int) (currentMeta >>> 32);
                int[] value = values[index];
                if (equals(value, prototype, stackSize, prefix, suffix)) {
                    return index + 1;
                }
            }
            i = (i + 1) & mask;
        }

        int[] stack = new int[stackSize + prefix.length + suffix.length];
        System.arraycopy(prefix, 0, stack, 0, prefix.length);
        System.arraycopy(prototype, 0, stack, prefix.length, stackSize);
        System.arraycopy(suffix, 0, stack, prefix.length + stackSize, suffix.length);

        values[size] = stack;
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

    private boolean equals(int[] a, int[] b, int bSize, int[] prefix, int[] suffix) {
        if (a.length != bSize + prefix.length + suffix.length) {
            return false;
        }
        if (!arraysRangeEquals(a, 0, prefix, prefix.length)) {
            return false;
        }
        if (!arraysRangeEquals(a, prefix.length, b, bSize)) {
            return false;
        }
        return arraysRangeEquals(a, prefix.length + bSize, suffix, suffix.length);
    }

    private boolean arraysRangeEquals(int[] left, int leftStart, int[] right, int length) {
        for (int i = 0; i < length; ++i) {
            if (left[leftStart + i] != right[i]) {
                return false;
            }
        }
        return true;
    }

    private static int murmur(int[] data, int size, int[] prefix, int[] suffix) {
        int m = 0x5bd1e995;
        int h = 0x9747b28c ^ (data.length + 1);

        for (int p : prefix) {
            int k = p * m;
            k ^= k >>> 24;
            k *= m;
            h *= m;
            h ^= k;
        }

        for (int i = 0; i < size; i++) {
            int k = data[i];
            k *= m;
            k ^= k >>> 24;
            k *= m;
            h *= m;
            h ^= k;
        }

        for (int s : suffix) {
            int k = s * m;
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
