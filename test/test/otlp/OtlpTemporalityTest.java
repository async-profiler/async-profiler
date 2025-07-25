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

        ValueType sampleType = getSampleType(profiler);
        assert sampleType.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_DELTA : 
            "First dump should be DELTA";

        sampleType = getSampleType(profiler);
        assert sampleType.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE : 
            "Second dump should be CUMULATIVE";

        sampleType = getSampleType(profiler);
        assert sampleType.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE : 
            "Third dump should be CUMULATIVE";

        profiler.stop();
        profiler.start(Events.CPU, 1_000_000);

        sampleType = getSampleType(profiler);
        assert sampleType.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_DELTA : 
            "After restart, first dump should be DELTA";

        sampleType = getSampleType(profiler);
        assert sampleType.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE : 
            "After restart, second dump should be CUMULATIVE";

        sampleType = getSampleType(profiler);
        assert sampleType.getAggregationTemporality() == AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE : 
            "After restart, third dump should be CUMULATIVE";

        profiler.stop();
    }

    public static ValueType getSampleType(AsyncProfiler profiler) throws Exception {
        byte[] dump = profiler.dumpOtlp();
        ProfilesData data = ProfilesData.parseFrom(dump);
        Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
        return profile.getSampleType(0);
    }

}
