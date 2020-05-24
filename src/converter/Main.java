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

import java.io.File;

/**
 * Main entry point of jar.
 * Invokes the proper converter depending on input/output file names.
 */
public class Main {

    public static void main(String[] args) throws Exception {
        if (args.length < 2) {
            System.out.println("Usage: java -jar converter.jar <input> <output>");
            System.out.println("Currently supported conversions:");
            System.out.println("  jfr -> nflx");
            System.exit(1);
        }

        String src = args[0];
        String dst = args[1];

        if (src.endsWith(".jfr") && (dst.endsWith(".nflx") || new File(dst).isDirectory())) {
            jfr2nflx.main(args);
        } else {
            System.out.println("Unrecognized input/output format");
            System.exit(1);
        }
    }
}
