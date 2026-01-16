/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public interface EventCollector {

    void collect(Event e);

    void beforeChunk();

    void afterChunk();

    // Returns true if this collector has remaining data to process
    boolean finish();

    void forEach(Visitor visitor);

    interface Visitor {
        void visit(Event event, long samples, long value);

        // Implementations can assume that ownership of 'timestamps' is released to the visitor
        default void visit(Event event, long samples, long value, long[] timestamps) {
            visit(event, samples, value);
        }
    }
}
