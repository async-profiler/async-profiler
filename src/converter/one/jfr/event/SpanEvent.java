/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import java.util.Objects;

public class SpanEvent extends Event {
    public final long duration;
    public final String tag;

    public SpanEvent(long time, int tid, long duration, String tag) {
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
        if (o instanceof SpanEvent) {
            SpanEvent s = (SpanEvent) o;
            return Objects.equals(tag, s.tag);
        }
        return false;
    }

    @Override
    public long value() {
        return duration;
    }
}
