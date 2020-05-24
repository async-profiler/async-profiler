/*
 * Copyright 2020 Andrei Pangin
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

package one.jfr;

public class Sample implements Comparable<Sample> {
    public final long time;
    public final int tid;
    public final int stackTraceId;
    public final short threadState;

    public Sample(long time, int tid, int stackTraceId, short threadState) {
        this.time = time;
        this.tid = tid;
        this.stackTraceId = stackTraceId;
        this.threadState = threadState;
    }

    @Override
    public int compareTo(Sample o) {
        return Long.compare(time, o.time);
    }
}
