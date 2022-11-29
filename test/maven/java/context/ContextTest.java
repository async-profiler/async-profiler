package context;

import org.junit.jupiter.api.Test;
import org.openjdk.jmc.common.IMCThread;
import org.openjdk.jmc.common.item.IAttribute;
import org.openjdk.jmc.common.item.IItem;
import org.openjdk.jmc.common.item.IItemCollection;
import org.openjdk.jmc.common.item.IItemIterable;
import org.openjdk.jmc.common.item.IMemberAccessor;
import org.openjdk.jmc.common.item.ItemFilters;
import org.openjdk.jmc.common.unit.IQuantity;
import org.openjdk.jmc.flightrecorder.JfrAttributes;
import org.openjdk.jmc.flightrecorder.JfrLoaderToolkit;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.ThreadLocalRandom;

import one.profiler.AsyncProfiler;
import utils.Utils;

import static org.openjdk.jmc.common.item.Attribute.attr;
import static org.openjdk.jmc.common.unit.UnitLookup.NUMBER;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

public class ContextTest {

  public static final IAttribute<IQuantity> LOCAL_ROOT_SPAN_ID = attr("localRootSpanId", "localRootSpanId",
      "localRootSpanId", NUMBER);
  public static final IAttribute<IQuantity> SPAN_ID = attr("spanId", "spanId",
      "spanId", NUMBER);

    @Test
    public void testReadContext() throws Exception {
        Path jfrDump = Files.createTempFile("context-test", ".jfr");
        AsyncProfiler ap = AsyncProfiler.getInstance(Utils.getAsyncProfilerLib());
        ap.addThread(ap.getNativeThreadId());
        ap.setContext(42, 84);
        ap.execute("start,cpu=1ms,jfr,thread,file=" + jfrDump.toAbsolutePath());
        // do some work to get some cpu samples
        long value = 0;
        for (int i = 0; i < 10_000_000; i++) {
           value ^= ThreadLocalRandom.current().nextLong();
        }
        System.err.println(value);
        ap.stop();
        IItemCollection events = JfrLoaderToolkit.loadEvents(jfrDump.toFile());
        IItemCollection cpuSamples = events.apply(ItemFilters.type("datadog.ExecutionSample"));
        assertTrue(cpuSamples.hasItems());
        for (IItemIterable cpuSample : cpuSamples) {
            IMemberAccessor<IQuantity, IItem> rootSpanIdAccessor = LOCAL_ROOT_SPAN_ID.getAccessor(cpuSample.getType());
            IMemberAccessor<IQuantity, IItem> spanIdAccessor = SPAN_ID.getAccessor(cpuSample.getType());
            IMemberAccessor<IMCThread, IItem> threadAccessor = JfrAttributes.EVENT_THREAD.getAccessor(cpuSample.getType());
            for (IItem sample : cpuSample) {
                if (threadAccessor.getMember(sample).getThreadName().equals(Thread.currentThread().getName())) {
                    long rootSpanId = rootSpanIdAccessor.getMember(sample).longValue();
                    assertEquals(84, rootSpanId);
                    long spanId = spanIdAccessor.getMember(sample).longValue();
                    assertEquals(42, spanId);
                }
            }
        }
    }

}
