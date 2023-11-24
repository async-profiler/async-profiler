/*
 * Copyright 2020 Andrei Pangin
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

import java.util.Arrays;

/**
 * Main entry point of jar.
 * Lists available converters.
 */
public class Main {

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            usage();
            return;
        }

        String[] converterArgs = Arrays.copyOfRange(args, 1, args.length);
        switch (args[0]) {
            case "FlameGraph":
                FlameGraph.main(converterArgs);
                break;
            case "jfr2flame":
                jfr2flame.main(converterArgs);
                break;
            case "jfr2nflx":
                jfr2nflx.main(converterArgs);
                break;
            case "jfr2pprof":
                jfr2pprof.main(converterArgs);
                break;
            default:
                System.out.println("Unknown converter: " + args[0] + "\n");
                usage();
                System.exit(1);
        }
    }

    private static void usage() {
        System.out.println("Usage: java -cp converter.jar <Converter> [options] <input> <output>");
        System.out.println();
        System.out.println("Available converters:");
        System.out.println("  FlameGraph  input.collapsed output.html");
        System.out.println("  jfr2flame   input.jfr       output.html");
        System.out.println("  jfr2nflx    input.jfr       output.nflx");
        System.out.println("  jfr2pprof   input.jfr       output.pprof");
    }
}
