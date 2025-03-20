/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import java.util.Objects;

public class Span extends Event {
    public final long duration;
    public final String tag;

    public Span(long time, int tid, long duration, String tag) {
        super(time, tid, 0);
        this.duration = duration;
        this.tag = tag;
    }

    @Override
    public int hashCode() {
        return tag != null ? tag.hashCode() : 0;
    }

    @Override
    public boolean sameGroup(Event o) {
        if (o instanceof Span) {
            Span s = (Span) o;
            return Objects.equals(tag, s.tag);
        }
        return false;
    }

    @Override
    public long value() {
        return duration;
    }
}
