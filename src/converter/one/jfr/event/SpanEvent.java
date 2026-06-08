/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import java.util.Objects;

public class SpanEvent extends IntervalEvent {
    public final String tag;

    public SpanEvent(long time, int tid, long duration, String tag) {
        super(time, tid, 0, duration);
        this.tag = tag;
    }

    @Override
    public int hashCode() {
        return Objects.hashCode(tag) * 127;
    }

    @Override
    public boolean sameGroup(Event o) {
        if (o instanceof SpanEvent) {
            SpanEvent c = (SpanEvent) o;
            return Objects.equals(tag, c.tag);
        }
        return false;
    }

    @Override
    public String tag() {
        return tag;
    }
}
