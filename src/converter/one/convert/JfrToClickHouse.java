/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package one.convert;

import java.io.BufferedOutputStream;
import java.io.BufferedWriter;
import java.io.Closeable;

import one.jfr.JfrClass;
import one.jfr.JfrField;
import one.jfr.JfrReader;
import one.jfr.Dictionary;
import one.jfr.event.Event;

import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

public class JfrToClickHouse implements Closeable {

    private static final Event UNIT = new Event(0L, 0, 0) {
    };
    private final Writer ddl;
    private final Writer ddl_flat;
    private final Writer ddl_drop;
    private final String dir;
    private final UUID recordingId;
    private final Map<String, OutputStream> outputs = new HashMap<>();
    private final Set<String> known_types = new HashSet<>();
    private final Set<String> dicts = new HashSet<>();
    private final JfrReader jfr;

    public static void convert(String input, String output, Arguments args) throws IOException {
        UUID recordingId = UUID.randomUUID();
        try (JfrToClickHouse converter = new JfrToClickHouse(input, output, recordingId)) {
            converter.process();
        }
    }

    private JfrToClickHouse(String fileName, String dir, UUID recordingId) throws IOException {
        Files.createDirectories(Paths.get(dir));

        ddl = new BufferedWriter(new FileWriter(dir + "/ddl.sql"));
        ddl_flat = new BufferedWriter(new FileWriter(dir + "/ddl_flat.sql"));
        ddl_drop = new BufferedWriter(new FileWriter(dir + "/ddl_drop.sql"));

        ddl.append("CREATE TABLE IF NOT EXISTS jfr_recording (date Date, nfs_dir LowCardinality(String), id UUID) ENGINE = MergeTree() ORDER BY (date, nfs_dir, id);\n");
        ddl.append("INSERT jfr_recording (date, nfs_dir, id) values (");
        ddl.append(Long.toString(System.currentTimeMillis())).append(",");
        ddl.append("'").append(fileName).append("'").append(",");
        ddl.append("'").append(recordingId.toString()).append("'");
        ddl.append(");\n");

        this.dir = dir;
        this.recordingId = recordingId;
        jfr = new JfrReader(fileName) {
            @Override
            protected void readConstants(JfrClass type) throws IOException {
                switch (type.name) {
                    case "jdk.types.ChunkHeader":
                        buf.position(buf.position() + (CHUNK_HEADER_SIZE + 3));
                        return;
                }

                dicts.add(type.name);
                int count = getVarint();
                // System.out.println("Reading " + count + " " + type.name);
                for (int i = 0; i < count; i++) {
                    transcodeRecord(type, true);
                }
            }

            @Override
            protected void cacheEventTypes() {
                super.cacheEventTypes();

                // mark referenced dicts
                // (in case they doesn't have values)
                types.forEach((i, type) -> {
                    for (JfrField field : type.fields) {
                        if (field.constantPool) {
                            dicts.add(this.types.get(field.type).name);
                        }
                    }
                });

                types.forEach((i, type) -> {
                    try {
                        if (known_types.add(type.name)) {
                            String name = table_name(type.name);
                            List<String> fields = new ArrayList<>();
                            List<String> order = new ArrayList<>();

                            fields.add("recording_id UUID");
                            order.add("recording_id");

                            if (dicts.contains(type.name)) {
                                fields.add("id Int64");
                                order.add("id");
                            }

                            for (JfrField field : type.fields) {
                                fields.add(ddl_name(field.name) + " " + toClickHouseType(field));
                                if ("startTime".equals(field.name)) {
                                    order.add("startTime");
                                }
                            }

                            // string doesn't have string field
                            if (type.name.equals("java.lang.String")) {
                                fields.add("value String");
                            }

                            ddl_drop.append("DROP TABLE " + name + ";\n");

                            ddl.append("CREATE TABLE IF NOT EXISTS ");
                            ddl.append(name);
                            ddl.append(" (");
                            ddl.append(String.join(", ", fields));
                            ddl.append(") ENGINE = MergeTree() ORDER BY (");
                            ddl.append(String.join(", ", order));
                            ddl.append(");\n");

                            StringBuilder join = new StringBuilder();
                            List<String> join_fields = new ArrayList<>();
                            join_recursivly("it", type, join, join_fields, new HashSet<>());
                            if (join.length() > 0) {
                                Collections.reverse(join_fields);
                                ddl_flat.append("select \n " + String.join(",\n ", join_fields) + "\nfrom " + name + " as it\n");
                                ddl_flat.append(join.toString());
                                ddl_flat.append(";\n\n");
                            }
                        }
                    } catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                });
            }

            protected <E extends Event> E doReadEvent(int type, Class<E> cls) throws IOException {
                JfrClass typeObj = types.get(type);
                if (typeObj == null) {
                    return null;
                }

                transcodeRecord(typeObj, false);
                return (E) UNIT;
            }

            private void transcodeRecord(JfrClass type, boolean constant) throws IOException {
                OutputStream output = outputs.get(type.name);
                if (output == null) {
                    output = new BufferedOutputStream(new FileOutputStream(dir + "/" + table_name(type.name) + ".rows"));
                    outputs.put(type.name, output);
                }
                RowBinaryWriter writer = new RowBinaryWriter(output);

                writer.writeUUID(recordingId);
                if (constant) {
                    writer.writeInt64(getVarlong());
                }

                for (JfrField field : type.fields) {
                    transcodeValue(field, writer);
                }

                // string doesn't have string field
                if (type.name.equals("java.lang.String")) {
                    writer.writeString(getString());
                }
            }

            private void join_recursivly(String parent, JfrClass type, StringBuilder join, List<String> fields, Set<JfrClass> parents) {
                if (!parents.add(type)) {
                    fields.add(parent + ".*");
                    return;
                }
                for (JfrField field : type.fields) {
                    String name = ddl_name(field.name);
                    String ref = parent + "." + name;
                    if (field.constantPool) {
                        JfrClass target = types.get(field.type);
                        String child = ref.replace(".", "__");
                        join.append("left join " + table_name(target.name) + " as " + child + " on ");
                        join.append(child + ".recording_id = " + parent + ".recording_id and ");
                        join.append(child + ".id = " + ref + " \n");
                        join_recursivly(child, target, join, fields, parents);
                    } else {
                        fields.add(ref);
                    }
                }
                parents.remove(type);
            }

            private void transcodeValue(JfrField field, RowBinaryWriter writer) throws IOException {
                if (field.dimension > 0) {
                    if (field.dimension > 1) {
                        System.err.println("Multidimensional arrays not supported");
                    }
                    int count = getVarint();
                    writer.writeUnsignedVarInt(count);
                    for (int i = 0; i < count; i++) {
                        transcodeScalar(field, writer);
                    }
                } else {
                    transcodeScalar(field, writer);
                }
            }

            private void transcodeScalar(JfrField field, RowBinaryWriter writer) throws IOException {
                if (field.constantPool) {
                    writer.writeInt64(getVarlong());
                    return;
                }

                switch (field.type) {
                    case 4:
                        writer.writeUInt8(getBoolean() ? 1 : 0);
                        break;
                    case 5:
                        writer.writeUInt16(getVarint());
                        break;
                    case 6:
                        writer.writeFloat32(getFloat());
                        break;
                    case 7:
                        writer.writeFloat64(getDouble());
                        break;
                    case 8:
                        writer.writeInt8(getByte());
                        break;
                    case 9:
                        writer.writeInt16(getVarint());
                        break;
                    case 10:
                        writer.writeInt32(getVarint());
                        break;
                    case 11:
                        writer.writeInt64(getVarlong());
                        break;
                    case 20:
                        writer.writeString(getString());
                        break;
                    default:
                        JfrClass type = types.get(field.type);
                        if (type != null) {
                            for (JfrField f : type.fields) {
                                transcodeValue(f, writer);
                            }
                        } else {
                            System.err.println("Value of unknown type " + field.type);
                            getVarlong(); // Skip unknown
                            writer.writeInt64(0);
                        }
                }
            }

            private String toClickHouseType(JfrField field) {
                if (field.dimension > 0) {
                    if (field.dimension > 1) {
                        System.err.println("Multidemensional arrays are note supported: " + field);
                    }
                    return "Array(" + getScalarType(field) + ")";
                }
                return getScalarType(field);
            }

            private String getScalarType(JfrField field) {
                if (field.constantPool) {
                    String target = types.get(field.type).name;
                    return "Int64 /* " + target + " */";
                }

                switch (field.type) {
                    case 4:
                        return "UInt8";
                    case 5:
                        return "UInt16";
                    case 6:
                        return "Float32";
                    case 7:
                        return "Float64";
                    case 8:
                        return "Int8";
                    case 9:
                        return "Int16";
                    case 10:
                        return "Int32";
                    case 11:
                        return "Int64";
                    case 20:
                        return "String";
                    default:
                        JfrClass type = types.get(field.type);
                        if (type != null) {
                            StringBuilder sb = new StringBuilder("Tuple(");
                            boolean first = true;
                            for (JfrField f : type.fields) {
                                if (!first) {
                                    sb.append(", ");
                                }
                                first = false;
                                sb.append(ddl_name(f.name)).append(" ").append(toClickHouseType(f));
                            }
                            sb.append(")");
                            return sb.toString();
                        }
                        return "String";
                }
            }
        };
    }

    private static String table_name(String name) {
        return "jfr_" + ddl_name(name);
    }

    private static String ddl_name(String name) {
        return name.replace('.', '_').replace('$', '_');
    }

    private void process() throws IOException {
        for (Event event; (event = jfr.readEvent(null)) != null;) {
        }
    }

    @Override
    public void close() throws IOException {
        ddl.flush();
        ddl_drop.flush();
        ddl_flat.flush();

        ddl.close();
        ddl_drop.close();
        ddl_flat.close();

        outputs.forEach((key, value) -> {
            try {
                value.flush();
                value.close();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        });
    }

    private class RowBinaryWriter {

        private final OutputStream out;

        public RowBinaryWriter(OutputStream out) {
            this.out = out;
        }

        public void writeUInt8(int v) throws IOException {
            out.write(v);
        }

        public void writeUInt16(int v) throws IOException {
            out.write(v & 0xFF);
            out.write((v >>> 8) & 0xFF);
        }

        public void writeInt8(int v) throws IOException {
            out.write(v);
        }

        public void writeInt16(int v) throws IOException {
            writeUInt16(v);
        }

        public void writeInt32(int v) throws IOException {
            out.write(v & 0xFF);
            out.write((v >>> 8) & 0xFF);
            out.write((v >>> 16) & 0xFF);
            out.write((v >>> 24) & 0xFF);
        }

        public void writeUInt64(long v) throws IOException {
            writeInt64(v);
        }

        public void writeInt64(long v) throws IOException {
            out.write((int) (v & 0xFF));
            out.write((int) ((v >>> 8) & 0xFF));
            out.write((int) ((v >>> 16) & 0xFF));
            out.write((int) ((v >>> 24) & 0xFF));
            out.write((int) ((v >>> 32) & 0xFF));
            out.write((int) ((v >>> 40) & 0xFF));
            out.write((int) ((v >>> 48) & 0xFF));
            out.write((int) ((v >>> 56) & 0xFF));
        }

        public void writeFloat32(float v) throws IOException {
            writeInt32(Float.floatToRawIntBits(v));
        }

        public void writeFloat64(double v) throws IOException {
            writeInt64(Double.doubleToRawLongBits(v));
        }

        public void writeString(String s) throws IOException {
            if (s == null) {
                writeVarInt(0);
            } else {
                byte[] bytes = s.getBytes(StandardCharsets.UTF_8);
                writeVarInt(bytes.length);
                out.write(bytes);
            }
        }

        public void writeUUID(UUID uuid) throws IOException {
            long msb = uuid.getMostSignificantBits();
            long lsb = uuid.getLeastSignificantBits();
            writeInt64(msb);
            writeInt64(lsb);
        }

        private void writeVarInt(long v) throws IOException {
            while ((v & 0xFFFFFFFFFFFFFF80L) != 0L) {
                out.write((int) ((v & 0x7F) | 0x80));
                v >>>= 7;
            }
            out.write((int) (v & 0x7F));
        }

        public void writeUnsignedVarInt(long v) throws IOException {
            while ((v & 0xFFFFFFFFFFFFFF80L) != 0L) {
                out.write((int) ((v & 0x7F) | 0x80));
                v >>>= 7;
            }
            out.write((int) (v & 0x7F));
        }
    }
}
