/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
package probe;

import java.io.BufferedReader;
import java.io.InputStreamReader;

public class NativeMappings {
    static native long exercise();
    static native void cleanup();
    public static void main(String[] args) throws Exception {
        BufferedReader input = new BufferedReader(new InputStreamReader(System.in));
        System.out.println("READY");
        input.readLine();
        // Intentionally load after attach to exercise the dlopen installation path.
        System.load(args[0]);
        System.out.println("DONE bytes=" + exercise());
        input.readLine();
        cleanup();
        System.out.println("CLEANED");
        input.readLine();
    }
}
