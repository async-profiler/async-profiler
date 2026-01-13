/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.util.ArrayList;

public class BidirectionalIndex<T> extends Index<T> {
    private final ArrayList<T> reverseIndex;

    public BidirectionalIndex(Class<T> cls, T empty) {
        this(cls, empty, 256);
    }

    public BidirectionalIndex(Class<T> cls, T empty, int initialCapacity) {
        super(cls, empty, initialCapacity);
        this.reverseIndex = new ArrayList<>(initialCapacity);
        this.reverseIndex.add(empty);
    }

    @Override
    public int index(T key) {
        assert super.size() == reverseIndex.size();
        int idx = super.index(key);
        if (idx < reverseIndex.size()) {
            // Key already exists
            return idx;
        }
        assert idx == reverseIndex.size();
        reverseIndex.add(key);
        return idx;
    }

    public T getKey(int idx) {
        return reverseIndex.get(idx);
    }
}
