package jfr;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.openjdk.jmc.common.item.IItem;
import org.openjdk.jmc.common.item.IItemCollection;
import org.openjdk.jmc.common.item.IMemberAccessor;
import org.openjdk.jmc.common.item.ItemFilters;
import org.openjdk.jmc.common.unit.IQuantity;
import org.openjdk.jmc.flightrecorder.JfrAttributes;
import org.openjdk.jmc.flightrecorder.JfrLoaderToolkit;

import one.profiler.AsyncProfiler;
import one.profiler.ContextIntervals;

public class ContextIntervalsTest {
    private AsyncProfiler profiler;
    private ContextIntervals contextIntervals;

    private static final byte startTimestamp = 5;
    private static final byte duration = 3;

    private final int chunkOffsetIdx = 3;
    private final int bitmapOffsetIdx = 12;
    private final byte chunkOffset = 9;
    private final byte bitmapOffset = 6;
    private final byte[] dataTemplate = new byte[] {
        0, 0, 0, 0, // 4 bytes for the offset to group varint encoded chunk
        1, // varint encoded start timestamp (1)
        1, // varint encoded frequency multuplier (1)
        1, // varint encoded number of threads (1)
        1, // varint encoded thread ID (1)
        1, // varint encoded number of intervals (1)
        0, 0, 0, 0, // 4 bytes for the offset to the group varint bitmap
        startTimestamp, // group varint encoded start timestamp of the 1st interval
        duration, // group varint encoded duration (timestamp delta) of the 1st interval
        0b0000_0000, 0, 0, 0 // group varint bitmap - 8 longs can be encoded by 3 bytes in this bitmap; minimal 4 bytes is used
    };

    private Path jfrPath;

    @BeforeEach
    void setup() throws Exception {
        String basedir = System.getProperty("basedir");
        if (basedir == null) {
            Path clzPath = new File(ContextIntervalsTest.class.getClassLoader().getResource("jfr/ContextIntervalsTest.class").toURI()).toPath();
            basedir = clzPath.getParent().getParent().getParent().getParent().toString();
        }
        Path libPath = Paths.get(basedir, "build", "libasyncProfiler.so");
        jfrPath = Files.createTempFile("test-", ".jfr");
        profiler = AsyncProfiler.getInstance(libPath.toString());
        profiler.execute("start,jfr=7,file=" + jfrPath.toString() + ",logLevel=info,event=cpu");
        contextIntervals = new ContextIntervals(profiler);
    }

    @AfterEach
    void teardown() throws Exception {
        try {
            profiler.stop();
        } catch (IllegalStateException ignored) {}
        Files.delete(jfrPath);
    }

    @Test
    void testNullValues() {
        assertEquals(1, contextIntervals.writeContextIntervals(null, null));
        assertEquals(1, contextIntervals.writeContextIntervals(null, ByteBuffer.wrap(new byte[0])));
        assertEquals(1, contextIntervals.writeContextIntervals("context", null));
    }

    /**
     * A simple attempt on fuzzing the input data to make sure no SIGSEGV is provoked even
     * by a totally garbage data.
     */
    @Test
    void testFuzzData() {
        System.out.print("Running fuzz tests: ");
        Random rnd = new Random();
        for (int i = 0; i < 100_000; i++) {
            int size = rnd.nextInt(1025);
            byte[] data = new byte[size];
            for (int j = 0; j < size; j++) {
                data[j] = (byte)((rnd.nextInt(256) - 128) & 0xff);
            }
            contextIntervals.writeContextIntervals("context", ByteBuffer.wrap(data));
            if (i % 10000 == 0) {
                System.out.print(".");
            }
        }
        System.out.println();
        System.out.println();
    }

    @Test
    void testZeroOffsets() {
        byte[] data = Arrays.copyOf(dataTemplate, dataTemplate.length);
        assertEquals(1, contextIntervals.writeContextIntervals("context", ByteBuffer.wrap(data)));

        data[chunkOffsetIdx] = chunkOffset;
        assertEquals(17, contextIntervals.writeContextIntervals("context", ByteBuffer.wrap(data)));
    }

    @Test
    void testValidOffsets() throws Exception {
        byte[] data = Arrays.copyOf(dataTemplate, dataTemplate.length);
        data[chunkOffsetIdx] = chunkOffset;
        data[bitmapOffsetIdx] = bitmapOffset;
        
        assertEquals(0, contextIntervals.writeContextIntervals("context", ByteBuffer.wrap(data)));
        
        // no events should be emitted from here since they are not crossing the threshold
        assertEquals(0, contextIntervals.writeContextIntervals("context", ByteBuffer.wrap(data), 10));

        profiler.stop();

        IItemCollection events = JfrLoaderToolkit.loadEvents(jfrPath.toFile());
        IItemCollection filtered = events.apply(ItemFilters.type("datadog.ContextInterval"));

        assertTrue(filtered.hasItems());

        List<Long> durations = new ArrayList<>();
        filtered.forEach(iterable -> {
            IMemberAccessor<IQuantity, IItem> durationAccessor = JfrAttributes.DURATION.getAccessor(iterable.getType());
            iterable.forEach(event -> {
                durations.add(durationAccessor.getMember(event).longValue());
            });
        });

        assertEquals(1, durations.size());
        assertEquals(duration, durations.get(0).byteValue());
    }
    
    @Test
    void testPersistedSanity() throws Exception {
        // persisted and encoded interval blob taken from a real life deployment - it contains 47 intervals
        int expectedIntervals = 47;
        String encoded = "AAAAIvb1iIiHwkvoBwiTAgGUAgGWAgFGDUcCVQFlAe4CGwAAANIs2DlgHjBMYJEXLl8F9uUrUcucNQGInnTTAwndBz7DCpqhagMQAeo+AXvDNIieDGCUFu4C3RN1CSwNvwUxW68NbxvvAasDdAGCCUgp69kBCK0BIJqNuS9/k15V1gHKg5YBxJlMOpIJQwPKAb2MrAMLHBsBslYoAjQBoAPEDtYC6AFkAUwb3QI6AXgBgAovAv4DQQF7GUID1miBAl4zJAlBDeICCAkWBgYRkw1LTvUfHgP6AaoBcQGSGLkBPwQvAlQCIwGwBUgBuwRwAS4IzUUUUUUiSmUSSSSSSUkUQ2SSSSSSSSSSSSSSSSSSSSSSSSSSQAA";
        
        byte[] data = java.util.Base64.getDecoder().decode(encoded);
        assertEquals(0, contextIntervals.writeContextIntervals("context", ByteBuffer.wrap(data)));
        
        profiler.stop();

        IItemCollection events = JfrLoaderToolkit.loadEvents(jfrPath.toFile());
        IItemCollection filtered = events.apply(ItemFilters.type("datadog.ContextInterval"));

        assertTrue(filtered.hasItems());

        AtomicInteger counter = new AtomicInteger(0);
        filtered.forEach(iterable -> {
            IMemberAccessor<IQuantity, IItem> durationAccessor = JfrAttributes.DURATION.getAccessor(iterable.getType());
            iterable.forEach(event -> {
                long value = durationAccessor.getMember(event).longValue();
                assertTrue(value >= 0);
                assertTrue(value < 60 * 1_000_000_000L * 3); // sanity check - the blob does not contain any intervals longer than 1 minute
                counter.incrementAndGet();
            });
        });
        assertEquals(expectedIntervals, counter.get());
    }

    @Test
    void testOutOfBoundsChunkOffset() {
        byte[] data = Arrays.copyOf(dataTemplate, dataTemplate.length);
        data[chunkOffsetIdx] = (byte)(dataTemplate.length + 1);
        
        assertEquals(1, contextIntervals.writeContextIntervals("context", ByteBuffer.wrap(data)));
    }

    @Test
    void testOutOfBoundsBitmapOffset() {
        byte[] data = Arrays.copyOf(dataTemplate, dataTemplate.length);
        data[chunkOffsetIdx] = chunkOffset;
        data[bitmapOffsetIdx] = (byte)(dataTemplate.length + 1);
        
        assertEquals(17, contextIntervals.writeContextIntervals("context", ByteBuffer.wrap(data)));
    }
}
