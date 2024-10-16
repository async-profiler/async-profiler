/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.*;
import java.util.regex.Pattern;

public class Arguments {
    public String title = "Flame Graph";
    public String highlight;
    public String output;
    public String state;
    public Pattern include;
    public Pattern exclude;
    public double minwidth;
    public double grain;
    public int skip;
    public boolean help;
    public boolean reverse;
    public boolean cpu;
    public boolean wall;
    public boolean alloc;
    public boolean live;
    public boolean lock;
    public boolean threads;
    public boolean classify;
    public boolean total;
    public boolean lines;
    public boolean bci;
    public boolean simple;
    public boolean norm;
    public boolean dot;
    public long from;
    public long to;
    public final List<String> files = new ArrayList<>();

    public Arguments(String... args) {
        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            String fieldName;
            if (arg.startsWith("--")) {
                fieldName = arg.substring(2);
            } else if (arg.startsWith("-") && arg.length() == 2) {
                fieldName = alias(arg.charAt(1));
            } else {
                files.add(arg);
                continue;
            }

            try {
                Field f = Arguments.class.getDeclaredField(fieldName);
                if ((f.getModifiers() & (Modifier.PRIVATE | Modifier.STATIC | Modifier.FINAL)) != 0) {
                    throw new IllegalArgumentException(arg);
                }

                Class<?> type = f.getType();
                if (type == String.class) {
                    f.set(this, args[++i]);
                } else if (type == boolean.class) {
                    f.setBoolean(this, true);
                } else if (type == int.class) {
                    f.setInt(this, Integer.parseInt(args[++i]));
                } else if (type == double.class) {
                    f.setDouble(this, Double.parseDouble(args[++i]));
                } else if (type == long.class) {
                    f.setLong(this, parseTimestamp(args[++i]));
                } else if (type == Pattern.class) {
                    f.set(this, Pattern.compile(args[++i]));
                }
            } catch (NoSuchFieldException | IllegalAccessException e) {
                throw new IllegalArgumentException(arg);
            }
        }
    }

    private static String alias(char c) {
        switch (c) {
            case 'h':
                return "help";
            case 'o':
                return "output";
            case 'r':
                return "reverse";
            case 'I':
                return "include";
            case 'X':
                return "exclude";
            case 't':
                return "threads";
            case 's':
                return "state";
            default:
                return String.valueOf(c);
        }
    }

    // Milliseconds or HH:mm:ss.S or yyyy-MM-dd'T'HH:mm:ss.S
    private static long parseTimestamp(String time) {
        if (time.indexOf(':') < 0) {
            return Long.parseLong(time);
        }

        GregorianCalendar cal = new GregorianCalendar();
        StringTokenizer st = new StringTokenizer(time, "-:.T");

        if (time.indexOf('T') > 0) {
            cal.set(Calendar.YEAR, Integer.parseInt(st.nextToken()));
            cal.set(Calendar.MONTH, Integer.parseInt(st.nextToken()) - 1);
            cal.set(Calendar.DAY_OF_MONTH, Integer.parseInt(st.nextToken()));
        }
        cal.set(Calendar.HOUR_OF_DAY, st.hasMoreTokens() ? Integer.parseInt(st.nextToken()) : 0);
        cal.set(Calendar.MINUTE, st.hasMoreTokens() ? Integer.parseInt(st.nextToken()) : 0);
        cal.set(Calendar.SECOND, st.hasMoreTokens() ? Integer.parseInt(st.nextToken()) : 0);
        cal.set(Calendar.MILLISECOND, st.hasMoreTokens() ? Integer.parseInt(st.nextToken()) : 0);

        return cal.getTimeInMillis();
    }
}
