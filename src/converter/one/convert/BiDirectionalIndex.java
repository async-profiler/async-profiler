/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.util.Arrays;

public class BiDirectionalIndex<T> extends Index<T> {
    private T[] reverseIndex;

    public BiDirectionalIndex(Class<T> cls, T empty) {
        this(cls, empty, 256);
    }

    public BiDirectionalIndex(Class<T> cls, T empty, int initialCapacity) {
        super(cls, empty, initialCapacity);
        // Super already checks if initial capacity is < 0
        initialCapacity = initialCapacity == 0 ? 1 : initialCapacity;
        this.reverseIndex = (T[]) new Object[initialCapacity];
        this.reverseIndex[0] = empty;
    }

    @Override
    public int index(T key) {
        int idx = super.index(key);
        if (idx >= reverseIndex.length) {
            this.reverseIndex = Arrays.copyOf(reverseIndex, reverseIndex.length * 2);
        }
        reverseIndex[idx] = key;
        return idx;
    }

    public T getKey(int idx) {
        return reverseIndex[idx];
    }
}
