/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import one.convert.Frame;

public class Method {

    public static final Method EMPTY = new Method(0, 0, -1, (byte) 0, false);

    public final int className;
    public final int methodName;
    public final int location;
    public final byte type;
    public final boolean start;

    public int frequency;
    // An identifier based on frequency ordering, more frequent methods will get a lower ID
    public int frequencyBasedId;
    public int index;

    Method(int className, int methodName, int location, byte type, boolean start) {
        this.className = className;
        this.methodName = methodName;
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
        if (methodName != method.methodName) return false;
        if (location != method.location) return false;
        if (type != method.type) return false;
        return start == method.start;
    }

    @Override
    public int hashCode() {
        int result = className;
        result = 31 * result + methodName;
        result = 31 * result + location;
        result = 31 * result + (int) type;
        result = 31 * result + (start ? 1 : 0);
        return result;
    }
}
