/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public interface IEventAggregator {
    void collect(Event e);

    void finishChunk();

    void resetChunk();

    void finish();

    void setFactor(double factor);

    void coarsen(double grain);

    void forEach(Visitor visitor);

    void forEach(ValueVisitor visitor);

    public interface Visitor {
        void visit(Event event, long samples, long value);
    }

    public interface ValueVisitor {
        void visit(Event event, long value);
    }
}