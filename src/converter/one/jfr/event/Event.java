/*
 * Copyright 2021 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package one.jfr.event;

public abstract class Event implements Comparable<Event> {
    public final long time;
    public final int tid;
    public final int stackTraceId;

    protected Event(long time, int tid, int stackTraceId) {
        this.time = time;
        this.tid = tid;
        this.stackTraceId = stackTraceId;
    }

    @Override
    public int compareTo(Event o) {
        return Long.compare(time, o.time);
    }

    @Override
    public int hashCode() {
        return stackTraceId;
    }

    public boolean sameGroup(Event o) {
        return getClass() == o.getClass();
    }

    public long value() {
        return 1;
    }
}
