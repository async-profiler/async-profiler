/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.util.ArrayList;

public class BiDirectionalIndex<T> extends Index<T> {
    private final ArrayList<T> reverseIndex;

    public BiDirectionalIndex(Class<T> cls, T empty) {
        this(cls, empty, 256);
    }

    public BiDirectionalIndex(Class<T> cls, T empty, int initialCapacity) {
        super(cls, empty, initialCapacity);
        // Super already checks if initial capacity is < 0
        initialCapacity = initialCapacity == 0 ? 1 : initialCapacity;
        this.reverseIndex = new ArrayList<>(initialCapacity);
        this.reverseIndex.add(empty);
    }

    @Override
    public int index(T key) {
        assert super.size() == reverseIndex.size();
        int idx = super.index(key);
        if (idx < reverseIndex.size()) {
        // This means the key already exists in the index and calling reverseIndex.add(idx, key) will corrupt it.
            return idx;
        }
        if (idx > reverseIndex.size()) {
            throw new IllegalStateException("Reverse index is out of sync");
        }
        reverseIndex.add(key);
        return idx;
    }

    public T getKey(int idx) {
        if (idx < 0 || idx >= reverseIndex.size()) {
            throw new IllegalArgumentException("Invalid index: " + idx);
        }
        T key = reverseIndex.get(idx);
        if (key == null) {
            throw new IllegalArgumentException("No key exists at index: " + idx);
        }
        return key;
    }
}
