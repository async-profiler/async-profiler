/*
 * Copyright 2021 Datadog, Inc
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

package one.profiler;

import java.util.Arrays;

public class MemLeakEntry {

    private final Object ref;
    private final int refSize;
    private final long age;

    public MemLeakEntry(Object ref, int refSize, long age) {
        this.ref = ref;
        this.refSize = refSize;
        this.age = age;
    }

    // accessors
    public Object ref() {
        return ref;
    }
    public int refSize() {
        return refSize;
    }
    public long age() {
        return age;
    }

    // helpers
    public String className() {
        return ref.getClass().getName();
    }
}
