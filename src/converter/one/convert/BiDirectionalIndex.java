package one.convert;

import java.util.HashMap;
import java.util.Map;

public class BiDirectionalIndex<T> extends Index<T> {
    private final Map<Integer, T> reverseIndex;

    public BiDirectionalIndex(Class<T> cls, T empty) {
        super(cls, empty);
        this.reverseIndex = new HashMap<>();
        this.reverseIndex.put(0, empty);
    }

    @Override
    public int index(T key) {
        int idx = super.index(key);
        reverseIndex.put(idx, key);
        return idx;
    }

    public T getKey(int idx) {
        return reverseIndex.get(idx);
    }
}
