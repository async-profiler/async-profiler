/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr;

import one.jfr.event.Event;

import java.io.IOException;
import java.util.List;

public interface IEventReader {
    <E extends Event> List<E> readAllEvents(Class<E> cls) throws IOException;

    <E extends Event> E readEvent(Class<E> cls) throws IOException;
}
