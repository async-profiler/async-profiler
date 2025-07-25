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
        
        for (int i = 0; i < 3; i++) {
            byte[] dump = profiler.dumpOtlp();
            ProfilesData data = ProfilesData.parseFrom(dump);
            Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
            ValueType sampleType = profile.getSampleType(0);
            
            AggregationTemporality expected = (i == 0) ? 
                AggregationTemporality.AGGREGATION_TEMPORALITY_DELTA : 
                AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
                
            assert sampleType.getAggregationTemporality() == expected : 
                "Dump " + i + " should be " + expected;
        }

        profiler.stop();
        profiler.start(Events.CPU, 1_000_000);
        
        
        for (int i = 0; i < 3; i++) {
            byte[] dump = profiler.dumpOtlp();
            ProfilesData data = ProfilesData.parseFrom(dump);
            Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
            ValueType sampleType = profile.getSampleType(0);
            
            AggregationTemporality expected = (i == 0) ? 
                AggregationTemporality.AGGREGATION_TEMPORALITY_DELTA : 
                AggregationTemporality.AGGREGATION_TEMPORALITY_CUMULATIVE;
                
            assert sampleType.getAggregationTemporality() == expected : 
                "After restart, dump " + i + " should be " + expected;
        }
        
        System.out.println("All temporality tests passed!");
        profiler.stop();
    }
}
