/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public interface IEventAggregator extends IEventAcceptor {
    void collect(Event e);

    void finishChunk();

    void resetChunk();

    void finish();

    void coarsen(double grain);
}
