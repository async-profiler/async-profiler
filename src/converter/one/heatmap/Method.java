/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import one.convert.Frame;

public class Method {

    public final int className;
    public final int name;
    public final int location;
    public final byte type;
    public final boolean start;

    final long originalMethodId;

    public int frequency;
    // An identifier based on frequency ordering, more frequent methods will get a lower ID
    public int frequencyBasedId;
    public int index;

    Method(int className, int name) {
        this(0, className, name, 0, Frame.TYPE_NATIVE, true);
    }

    Method(long originalMethodId, int className, int name, int location, byte type, boolean start) {
        this.originalMethodId = originalMethodId;
        this.className = className;
        this.name = name;
        this.location = location;
        this.type = type;
        this.start = start;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Method method = (Method) o;

        if (className != method.className) return false;
        if (name != method.name) return false;
        if (location != method.location) return false;
        if (type != method.type) return false;
        return start == method.start;
    }

    @Override
    public int hashCode() {
        int result = className;
        result = 31 * result + name;
        result = 31 * result + location;
        result = 31 * result + (int) type;
        result = 31 * result + (start ? 1 : 0);
        return result;
    }
}
