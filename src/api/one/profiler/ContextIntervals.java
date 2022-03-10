package one.profiler;

import java.nio.ByteBuffer;

/**
 * A helper class to allow storing context intervals as events in the async-profiler JFR recording.
 */
@Datadog
public final class ContextIntervals {
    private final AsyncProfiler profiler;
    /**
     * The instance must be initialized with a valid {@linkplain AsyncProfiler}
     */
    public ContextIntervals(AsyncProfiler profiler) {
        this.profiler = profiler;
    }

    /**
     * Write the context intervals contained in the {@code intervalBlob} binary data
     * with the associated context.
     * 
     * @param context the associated context
     * @param intervalBlob the context interval binary blob (custom format)
     * @return returns exit code (0 = success)
     */
    public int writeContextIntervals(String context, ByteBuffer intervalBlob) {
        return writeContextIntervals(context, intervalBlob, 0L);
    }

    /**
     * Write the context intervals contained in the {@code intervalBlob} binary data
     * with the associated context, given that they pass the provided threshold.
     * 
     * @param context the associated context
     * @param intervalBlob the context interval binary blob (custom format)
     * @threshold intervals shorter than the threshold will not be emitted as JFR events
     * @return returns exit code (0 = success)
     */
    public int writeContextIntervals(String context, ByteBuffer intervalBlob, long threshold) {
        if (context == null || intervalBlob == null) {
            return 1;
        }
        
        if (intervalBlob.hasArray()) {
            // fast path - use the backing array directly
            int len = intervalBlob.position() > 0 ? intervalBlob.position() : intervalBlob.limit();
            return profiler.writeContextIntervals0(context, intervalBlob.array(), len, threshold);
        } else {
            if (intervalBlob.position() > 0) {
                intervalBlob = (ByteBuffer)intervalBlob.flip();
            }
            byte[] blobArray = new byte[intervalBlob.limit()];
            intervalBlob.get(blobArray);
            return profiler.writeContextIntervals0(context, blobArray, blobArray.length, threshold);
        }
    }
}
