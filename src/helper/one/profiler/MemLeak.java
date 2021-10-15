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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.function.Consumer;

public class MemLeak {

    private static final List<Consumer<Collection<MemLeakEntry>>> listeners = new ArrayList<>();

    public static void addListener(Consumer<Collection<MemLeakEntry>> listener) {
        listeners.add(listener);
    }

    public static void removeListener(Consumer<Collection<MemLeakEntry>> listener) {
        listeners.remove(listener);
    }

    private static void process(MemLeakEntry[] entries, int nentries) {
        // There is a chance for a race in native where some of entries is `null`,
        //  `nentries` is the source of truth over `entries.length` here.
        // Resize to make manipulating `entries` easier and more Java-like.
        entries = Arrays.copyOf(entries, nentries);

        Collection<MemLeakEntry> entriesList = Collections.unmodifiableCollection(Arrays.asList(entries));
        for (Consumer<Collection<MemLeakEntry>> listener : listeners) {
            listener.accept(entriesList);
        }
    }
}
