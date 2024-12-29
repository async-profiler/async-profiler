/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.io.IOException;
import java.io.PrintStream;
import java.util.Arrays;
import java.util.Comparator;

import one.convert.Arguments;
import one.convert.Index;
import one.convert.JfrConverter;
import one.convert.ResourceProcessor;
import one.jfr.DictionaryInt;

public class Heatmap {

    // TODO: should be probably an argument,
    // but there is a good chance that changing it will have some side effects
    public static final int BLOCK_DURATION_MS = 20;

    private final Arguments args;
    private State state;
    private long startMs;

    public Heatmap(Arguments args, JfrConverter converter) {
        this.args = args;
        this.state = new State(converter, BLOCK_DURATION_MS);
    }

    public void addEvent(int stackTraceId, int extra, byte type, long timeMs) {
        state.addEvent(stackTraceId, extra, type, timeMs);
    }

    public void addStack(long id, long[] methods, int[] locations, byte[] types, int size) {
        state.addStack(id, methods, locations, types, size);
    }

    public void beforeChunk() {
        state.methodsCache.clear();
    }

    public void finish(long startMs) {
        this.startMs = startMs;
        state.methodsCache.clear();
        state.stackTracesCache.clear();
    }

    private EvaluationContext evaluate() {
        State state = this.state;
        this.state = null;
        return new EvaluationContext(
                state.sampleList.samples(),
                state.methodsCache.methodsIndex(),
                state.stackTracesRemap.orderedTraces(),
                state.methodsCache.orderedSymbolTable()
        );
    }

    private void compressMethods(HtmlOut out, Method[] methods) {
        out.writeVar(methods.length);
        for (Method method : methods) {
            out.writeVar(method.className);
            out.writeVar(method.methodName);
            out.write18(method.location & 0xffff);
            out.write18(method.location >>> 16);
            out.write6(method.type);
        }
    }

    public void dump(PrintStream stream) throws IOException {
        if (state.sampleList.getRecordsCount() == 0) {
            // Need a better way to handle this, but we should not throw an exception
            stream.println("No samples found");
            return;
        }

        EvaluationContext evaluationContext = evaluate();

        String tail = ResourceProcessor.getResource("/heatmap.html");

        tail = ResourceProcessor.printTill(stream, tail, "/*executionsHeatmap:*/");
        HtmlOut out = new HtmlOut(stream);
        stream.print('S');
        printHeatmap(out, evaluationContext);
        stream.print('E');

        tail = ResourceProcessor.printTill(stream, tail, "/*methods:*/");
        out.reset();
        stream.print('S');
        printMethods(out, evaluationContext);
        stream.print('E');

        tail = ResourceProcessor.printTill(stream, tail, "/*title:*/");
        stream.print(args.title == null ? "Heatmap" : args.title);

        tail = ResourceProcessor.printTill(stream, tail, "/*startMs:*/0");
        stream.print(startMs);

        tail = ResourceProcessor.printTill(stream, tail, "/*cpool:*/");
        printConstantPool(stream, evaluationContext);

        stream.print(tail);
    }

    private void printHeatmap(final HtmlOut out, EvaluationContext context) {
        int veryStart = out.pos();
        int wasPos = out.pos();

        // calculates methods frequency during building the tree
        int[] stackChunksBuffer = buildLz78TreeAndPrepareData(context);

        // gives methods new ids, more frequent (in tree's data) methods will have lower id
        renameMethodsByFrequency(context);

        // writes "starts" - ids of methods that indicates a start of a next stack trace
        writeStartMethods(out, context);
        wasPos = debugStep("start methods", out, wasPos, veryStart);

        // writes block sizes, compressed by huffman algorithm
        writeBlockSizes(out, context);
        wasPos = debugStep("stack sizes", out, wasPos, veryStart);

        // NOTE: destroys internal state!
        SynonymTable synonymTable = context.nodeTree.extractSynonymTable();
        synonymTable.calculateSynonyms();
        // writes frequent lz tree nodes as a synonyms table
        writeSynonymsTable(out, synonymTable);
        wasPos = debugStep("tree synonyms", out, wasPos, veryStart);

        // writes lz tree with two pairs of var-ints: [parent node id] + [method id of this node]
        writeTree(out, synonymTable, context);
        wasPos = debugStep("tree body", out, wasPos, veryStart);

        // calculate counts for the next synonyms table, that will be used for samples
        int chunksCount = calculateSamplesSynonyms(synonymTable, context, stackChunksBuffer);
        // writes frequent lz tree nodes as a synonyms table (for sample chunks)
        writeSynonymsTable(out, synonymTable);
        wasPos = debugStep("samples synonyms", out, wasPos, veryStart);

        // writes sample chunks as var-ints references for [node id]
        writeSamples(out, synonymTable, context, stackChunksBuffer);
        debugStep("samples body", out, wasPos, veryStart);
        debug("storage size: " + context.nodeTree.storageSize());

        out.write30(context.nodeTree.nodesCount());
        out.write30(context.sampleList.blockSizes.length);
        out.write30(context.nodeTree.storageSize());
        out.write30(chunksCount);
        out.write30(context.sampleList.stackIds.length);
    }

    private void writeSamples(HtmlOut out, SynonymTable synonymTable, EvaluationContext context,
                              int[] stackChunksBuffer) {
        for (int stackId : context.sampleList.stackIds) {
            int chunksStart = stackChunksBuffer[stackId * 2];
            int chunksEnd = stackChunksBuffer[stackId * 2 + 1];

            for (int i = chunksStart; i < chunksEnd; i++) {
                int nodeId = stackChunksBuffer[i];
                out.writeVar(synonymTable.nodeIdOrSynonym(nodeId));
            }
        }
    }

    private int calculateSamplesSynonyms(SynonymTable synonymTable, EvaluationContext context,
                                         int[] stackChunksBuffer) {
        int chunksCount = 0;
        int[] childrenCount = synonymTable.reset();

        for (int stackId : context.sampleList.stackIds) {
            int chunksStart = stackChunksBuffer[stackId * 2];
            int chunksEnd = stackChunksBuffer[stackId * 2 + 1];

            for (int i = chunksStart; i < chunksEnd; i++) {
                childrenCount[stackChunksBuffer[i]]--; // negation for reverse sort
                chunksCount++;
            }
        }

        synonymTable.calculateSynonyms();
        return chunksCount;
    }

    private void writeTree(HtmlOut out, SynonymTable synonymTable, EvaluationContext context) {
        long[] data = context.nodeTree.treeData();
        int dataSize = context.nodeTree.treeDataSize();
        for (int i = 0; i < dataSize; i++) {
            long d = data[i];
            int parentId = context.nodeTree.extractParentId(d);
            int methodId = context.nodeTree.extractMethodId(d);

            out.writeVar(synonymTable.nodeIdOrSynonym(parentId));
            out.writeVar(context.orderedMethods[methodId].frequencyOrNewMethodId);
        }
    }

    private void writeSynonymsTable(HtmlOut out, SynonymTable synonymTable) {
        out.writeVar(synonymTable.synonymsCount());
        for (int i = 0; i < synonymTable.synonymsCount(); i++) {
            out.writeVar(synonymTable.synonymAt(i));
        }
    }

    private void writeStartMethods(HtmlOut out, EvaluationContext context) {
        int startsCount = 0;
        for (Method method : context.orderedMethods) {
            if (method.start) {
                startsCount++;
            }
        }
        out.writeVar(startsCount);
        for (Method method : context.orderedMethods) {
            if (method.start) {
                out.writeVar(method.frequencyOrNewMethodId);
            }
        }
    }

    private void renameMethodsByFrequency(EvaluationContext context) {
        Arrays.sort(context.orderedMethods, new Comparator<Method>() {
            @Override
            public int compare(Method o1, Method o2) {
                return Integer.compare(o2.frequencyOrNewMethodId, o1.frequencyOrNewMethodId);
            }
        });

        for (int i = 0; i < context.orderedMethods.length; i++) {
            Method method = context.orderedMethods[i];
            method.frequencyOrNewMethodId = i + 1; // zero is reserved for no method
        }

        // restores order
        context.methods.keys(context.orderedMethods);
    }

    private int[] buildLz78TreeAndPrepareData(EvaluationContext context) {
        int[] samples = context.sampleList.stackIds;

        // prepared data for output, firstly used to remember last stack positions
        int[] stackBuffer = new int[(context.stackTraces.length + 1) * 16];

        // remember the last position of stackId
        for (int i = 0; i < samples.length; i++) {
            int stackId = samples[i];
            stackBuffer[stackId * 2] = ~i;   // rewrites data multiple times, the last one wins
        }

        int chunksIterator = context.stackTraces.length * 2 + 1;

        // builds the tree and prepares data for the last stack
        for (int i = 0; i < samples.length; i++) {
            int stackId = samples[i];
            int current = 0;
            int[] stack = context.stackTraces[stackId];

            if (i == ~stackBuffer[stackId * 2]) {    // last version of that stack
                stackBuffer[stackId * 2] = chunksIterator;  // start

                for (int methodId : stack) {
                    current = context.nodeTree.appendChild(current, methodId);
                    if (current == 0) { // so we are starting from root again, it will be written to output as Lz78 element - [parent node id; method id]
                        context.orderedMethods[methodId].frequencyOrNewMethodId++;
                        if (stackBuffer.length == chunksIterator) {
                            stackBuffer = Arrays.copyOf(stackBuffer, chunksIterator + chunksIterator / 2);
                        }

                        int justAppendedId = context.nodeTree.nodesCount() - 1;
                        stackBuffer[chunksIterator++] = justAppendedId;
                        context.nodeTree.markNodeAsLastlyUsed(justAppendedId);
                    }
                }

                if (current != 0) {
                    if (stackBuffer.length == chunksIterator) {
                        stackBuffer = Arrays.copyOf(stackBuffer, chunksIterator + chunksIterator / 2);
                    }

                    stackBuffer[chunksIterator++] = current;
                    context.nodeTree.markNodeAsLastlyUsed(current);
                }

                stackBuffer[stackId * 2 + 1] = chunksIterator;  // end
            } else { // general case
                for (int methodId : stack) {
                    current = context.nodeTree.appendChild(current, methodId);
                    if (current == 0) { // so we are starting from root again, it will be written to output as Lz78 element - [parent node id; method id]
                        context.orderedMethods[methodId].frequencyOrNewMethodId++;
                    }
                }
            }
        }

        // removes unused chunks
        context.nodeTree.compactTree(stackBuffer, context.stackTraces.length * 2 + 1, chunksIterator);

        return stackBuffer;
    }

    private void writeBlockSizes(HtmlOut out, EvaluationContext context) {
        int[] blockSizeFrequencies = new int[1024];
        int maxBlockSize = 0;
        for (int blockSize : context.sampleList.blockSizes) {
            if (blockSize >= blockSizeFrequencies.length) {
                blockSizeFrequencies = Arrays.copyOf(blockSizeFrequencies, blockSize * 2);
            }
            blockSizeFrequencies[blockSize]++;
            maxBlockSize = Math.max(maxBlockSize, blockSize);
        }

        HuffmanEncoder encoder = new HuffmanEncoder(blockSizeFrequencies, maxBlockSize);

        long[] decodeTable = encoder.calculateOutputTable();

        out.writeVar(decodeTable.length);
        int maxBits = (int) (decodeTable[decodeTable.length - 1] >>> 56);
        out.writeVar(maxBits);

        for (long l : decodeTable) {
            out.writeVar(l & 0x00FF_FFFF_FFFF_FFFFL);
            out.writeVar(l >>> 56);
        }

        for (int blockSize : context.sampleList.blockSizes) {
            if (encoder.append(blockSize)) {
                for (int value : encoder.values) {
                    out.nextByte(value);
                }
            }
        }
        if (encoder.flushIfNeed()) {
            for (int value : encoder.values) {
                out.nextByte(value);
            }
        }
    }

    private void printConstantPool(PrintStream out, EvaluationContext evaluationContext) {
        for (String symbol : evaluationContext.symbols) {
            out.print('"');
            out.print(symbol.replace("\\", "\\\\").replace("\"", "\\\""));
            out.print("\",");
        }
    }

    private void printMethods(HtmlOut out, EvaluationContext evaluationContext) throws IOException {
        debug("methods count " + evaluationContext.orderedMethods.length);
        Arrays.sort(evaluationContext.orderedMethods, new Comparator<Method>() {
            @Override
            public int compare(Method o1, Method o2) {
                return Integer.compare(o1.frequencyOrNewMethodId, o2.frequencyOrNewMethodId);
            }
        });
        out.nextByte('A');
        compressMethods(out, evaluationContext.orderedMethods);
        out.nextByte('A');
    }

    private int debugStep(String step, HtmlOut out, int wasPos, int veryStartPos) {
        int nowPos = out.pos();
        debug(step + " " + (nowPos - wasPos) / (1024.0 * 1024.0) + " MB");
        debug(step + " pos in data " + (nowPos - veryStartPos));
        return nowPos;
    }

    private void debug(String text) {
        // Basically, no user will ever need that, but it will be helpful to debug broken data
        // System.out.println(text);
    }

    private static class EvaluationContext {
        final Index<Method> methods;
        final Method[] orderedMethods;
        final int[][] stackTraces;
        final String[] symbols;

        final SampleList.Result sampleList;

        final LzNodeTree nodeTree = new LzNodeTree();

        EvaluationContext(SampleList.Result sampleList, Index<Method> methods, int[][] stackTraces, String[] symbols) {
            this.sampleList = sampleList;
            this.methods = methods;
            this.stackTraces = stackTraces;
            this.symbols = symbols;

            orderedMethods = methods.keys();
        }
    }

    private static class State {

        private static final int LIMIT = Integer.MAX_VALUE;

        final SampleList sampleList;
        final StackStorage stackTracesRemap = new StackStorage();

        final DictionaryInt stackTracesCache = new DictionaryInt();
        final MethodCache methodsCache;

        // reusable array to (temporary) store (potentially) new stack trace
        int[] cachedStackTrace = new int[4096];

        State(JfrConverter converter, long blockDurationMs) {
            sampleList = new SampleList(blockDurationMs);
            methodsCache = new MethodCache(converter);
        }

        public void addEvent(int stackTraceId, int extra, byte type, long timeMs) {
            if (sampleList.getRecordsCount() >= LIMIT) {
                return;
            }
            if (extra == 0) {
                sampleList.add(stackTracesCache.get(stackTraceId), timeMs);
                return;
            }

            int id = stackTracesCache.get((long) extra << 32 | stackTraceId, -1);
            if (id != -1) {
                sampleList.add(id, timeMs);
                return;
            }

            int prototypeId = stackTracesCache.get(stackTraceId);
            int[] prototype = stackTracesRemap.get(prototypeId);

            id = stackTracesRemap.indexWithPrototype(prototype, methodsCache.indexForClass(extra, type));
            stackTracesCache.put((long) extra << 32 | stackTraceId, id);

            sampleList.add(id, timeMs);
        }

        public void addStack(long id, long[] methods, int[] locations, byte[] types, int size) {
            int[] stackTrace = cachedStackTrace;
            if (stackTrace.length < size) {
                cachedStackTrace = stackTrace = new int[size * 2];
            }

            for (int i = size - 1; i >= 0; i--) {
                long methodId = methods[i];
                byte type = types[i];
                int location = locations[i];

                int index = size - 1 - i;
                boolean firstMethodInTrace = index == 0;

                stackTrace[index] = methodsCache.index(methodId, location, type, firstMethodInTrace);
            }

            stackTracesCache.put(id, stackTracesRemap.index(stackTrace, size));
        }

    }

}
