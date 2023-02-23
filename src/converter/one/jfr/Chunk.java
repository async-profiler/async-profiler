package one.jfr;

import one.jfr.event.Event;

import java.util.Collections;
import java.util.Iterator;
import java.util.List;

public class Chunk<E extends Event> implements Iterable<E> {
    private final List<E> events;

    final int nextChunkPos;

    private Chunk(Builder<E> bld) {
        this.events = bld.events;
        this.nextChunkPos = bld.nextChunkPos;
    }

    public boolean isEmpty() {
        return this.events == null || this.events.size() == 0;
    }

    @Override
    public Iterator<E> iterator() {
        return Collections.unmodifiableList(events).iterator();
    }

    public static <E extends Event> Builder<E> builder() {
        return new Builder<>();
    }

    public static final class Builder<E extends Event> {
        private int nextChunkPos = -1;
        private List<E> events;

        public Builder() {
        }

        Builder<E> events(List<E> events) {
            this.events = events;
            return this;
        }

        Builder<E> nextChunkPos(int pos) {
            this.nextChunkPos = pos;
            return this;
        }

        Chunk<E> build() {
            return new Chunk<>(this);
        }
    }
}
