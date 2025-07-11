/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import one.convert.Frame;

public class FrameDesc {

    public final int className;
    public final int name;
    public final int location;
    public final byte type;
    public final boolean start;

    final long originalMethodId;

    FrameDesc next;

    public int frequencyOrNewMethodId;
    public int index;

    FrameDesc(int className, int name) {
        this(0, className, name, 0, Frame.TYPE_NATIVE, true);
    }

    FrameDesc(long originalMethodId, int className, int name, int location, byte type, boolean start) {
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

        FrameDesc frameDesc = (FrameDesc) o;

        if (className != frameDesc.className) return false;
        if (name != frameDesc.name) return false;
        if (location != frameDesc.location) return false;
        if (type != frameDesc.type) return false;
        return start == frameDesc.start;
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
