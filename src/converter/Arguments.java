/*
 * Copyright 2022 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.StringTokenizer;
import java.util.regex.Pattern;

class Arguments {
    String title = "Flame Graph";
    String highlight;
    Pattern include;
    Pattern exclude;
    double minwidth;
    int skip;
    boolean reverse;
    boolean cpu;
    boolean alloc;
    boolean lock;
    boolean threads;
    boolean total;
    boolean lines;
    boolean bci;
    boolean simple;
    boolean dot;
    boolean collapsed;
    long from;
    long to;
    String input;
    String output;

    Arguments(String... args) {
        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            if (arg.startsWith("--")) {
                try {
                    Field f = Arguments.class.getDeclaredField(arg.substring(2));
                    if ((f.getModifiers() & (Modifier.PRIVATE | Modifier.STATIC | Modifier.FINAL)) != 0) {
                        throw new IllegalStateException(arg);
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
            } else if (!arg.isEmpty()) {
                if (input == null) {
                    input = arg;
                } else {
                    output = arg;
                }
            }
        }
    }

    // Milliseconds or HH:mm:ss.S or yyyy-MM-dd'T'HH:mm:ss.S
    private long parseTimestamp(String time) {
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
