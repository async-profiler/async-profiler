package test.otlp;

import one.profiler.AsyncProfiler;
import one.profiler.Events;
import one.profiler.Counter;
import java.nio.ByteBuffer;
import io.opentelemetry.proto.profiles.v1development.*;

public class OtlpTemporalityTest {

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        profiler.start(Events.CPU, 1_000_000);

        assert dumpAndGetSampleType(profiler).getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_DELTA;
        assert dumpAndGetSampleType(profiler).getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
        assert dumpAndGetSampleType(profiler).getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;

        profiler.stop();
        profiler.start(Events.CPU, 1_000_000);

        assert dumpAndGetSampleType(profiler).getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_DELTA;
        assert dumpAndGetSampleType(profiler).getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
        assert dumpAndGetSampleType(profiler).getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;

        profiler.stop();
    }

    public static ValueType dumpAndGetSampleType(AsyncProfiler profiler) throws Exception {
        byte[] dump = profiler.dumpOtlp();
        ProfilesData data = ProfilesData.parseFrom(dump);
        Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
        return profile.getSampleType(0);
    }

}
