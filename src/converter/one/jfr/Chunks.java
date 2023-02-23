package one.jfr;

import one.jfr.event.Event;

import java.io.IOException;
import java.util.Iterator;
import java.util.NoSuchElementException;

public class Chunks<E extends Event> implements Iterable<Chunk<E>> {
    private final JfrReader reader;

    private Chunk<E> c;

    private final Class<E> cls;

    public Chunks(JfrReader reader) {
        this(reader, null);
    }

    public Chunks(JfrReader reader, Class<E> cls) {
        this.reader = reader;
        this.cls = cls;
        this.c = null;
    }

    @Override
    public Iterator<Chunk<E>> iterator() {
        return new Iterator<Chunk<E>>() {
            @Override
            public boolean hasNext() {
                try {
                    if (c == null)
                        return reader.ensureBytes(JfrReader.CHUNK_HEADER_SIZE);
                    return c.nextChunkPos != -1;
                } catch (IOException ioEx) {
                    return false;
                }
            }

            @Override
            public Chunk<E> next() {
                try {
                    c = reader.nextChunk(c == null ? 0 : c.nextChunkPos, cls);
                    if (c == null) {
                        throw new NoSuchElementException("Incomplete JFR file");
                    }
                    return c;
                } catch (IOException ioEx) {
                    throw new NoSuchElementException("fail to read event");
                }
            }
        };
    }
}
