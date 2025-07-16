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

    public int index(int[] prototype, int stackSize, int prefix, int suffix) {
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

        int[] stack = new int[stackSize + (prefix == 0 ? 0 : 1) + (suffix == 0 ? 0 : 1)];
        if (prefix != 0) {
            stack[0] = prefix;
            System.arraycopy(prototype, 0, stack, 1, stackSize);
        } else {
            System.arraycopy(prototype, 0, stack, 0, stackSize);
        }
        if (suffix != 0) {
            stack[prototype.length] = suffix;
        }

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

    private boolean equals(int[] a, int[] b, int bSize, int prefix, int suffix) {
        if (a.length != bSize + (prefix == 0 ? 0 : 1) + (suffix == 0 ? 0 : 1)) {
            return false;
        }
        int aIndex = 0;
        if (prefix != 0 && a[aIndex++] != prefix) {
            return false;
        }
        for (int bIndex = 0; bIndex < bSize; ++bIndex) {
            if (a[aIndex++] != b[bIndex]) {
                return false;
            }
        }
        return suffix != 0 && a[aIndex] == suffix;
    }

    private static int murmur(int[] data, int size, int prefix, int suffix) {
        int m = 0x5bd1e995;
        int h = 0x9747b28c ^ (data.length + 1);

        if (prefix != 0) {
            int k = prefix * m;
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

        if (suffix != 0) {
            int k = suffix * m;
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
