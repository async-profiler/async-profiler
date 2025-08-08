/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.lang.reflect.Array;
import java.util.HashMap;

/**
 * Container which records the index of appearance of the value it holds.
 * <p>
 * Allows retrieving the index of a given object in constant time, as well as
 * an ordered list of all values seen.
 * <p>
 * The object at index 0 is always the empty object.
 *
 * @param <T> type of the objects held in the container.
 */
public class Index<T> extends HashMap<T, Integer> {
    private final Class<T> cls;

    public Index(Class<T> cls, T empty) {
        this(cls, empty, 256);
    }

    public Index(Class<T> cls, T empty, int initialCapacity) {
        super(initialCapacity);
        this.cls = cls;
        super.put(empty, 0);
    }

    public int index(T key) {
        Integer index = super.get(key);
        if (index != null) {
            return index;
        } else {
            int newIndex = super.size();
            super.put(key, newIndex);
            return newIndex;
        }
    }

    @SuppressWarnings("unchecked")
    public T[] keys() {
        T[] result = (T[]) Array.newInstance(cls, size());
        for (Entry<T, Integer> entry : entrySet()) {
            result[entry.getValue()] = entry.getKey();
        }
        return result;
    }
}
