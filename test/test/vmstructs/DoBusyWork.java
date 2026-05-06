/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.vmstructs;

import java.util.ArrayList;
import java.util.Random;

public class DoBusyWork {
    public static void main(String[] args) {
        ArrayList<Integer> collectedNumbers = new ArrayList<>();
        for (int i = 0; i < 10; ++i) {
            ArrayList<Integer> numbers = doBusyWork();
            collectedNumbers.addAll(numbers);
        }
        collectedNumbers.sort(null);
    }

    public static ArrayList<Integer> doBusyWork() {
        long seed = System.currentTimeMillis();
        Random random = new Random(seed);
        ArrayList<Integer> numbers = new ArrayList<>();
        for (int i = 0; i < 1000000; i++) {
            numbers.add(random.nextInt());
        }
        numbers.sort(null);
        return numbers;
    }
}
