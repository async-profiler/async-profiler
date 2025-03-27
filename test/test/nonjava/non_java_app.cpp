/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common_non_java.h"

void validateArgsCount(int argc, int exepected, std::string context) {
    if (argc < exepected) {
        fprintf(stderr, "Too few arguments: %s\n", (char*) context.c_str());
        exit(1);
    }  
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the JVM
4. Start the profiler
5. Execute the JVM task
6. Stop the profiler
7. Stop the JVM

Expected output:
The profiler should be able to profile the JVM task

Explaination:
The JVM is loaded and started before the profiling session is started so it's attached correctly at the session start
*/
void testFlow1(int argc, char** argv) {
    CommonNonJava::loadProfiler();

    CommonNonJava::loadJvmLib();

    CommonNonJava::startJvm();

    CommonNonJava::startProfiler();

    CommonNonJava::executeJvmTask();

    CommonNonJava::stopProfiler(argv[2]);

    CommonNonJava::stopJvm();
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the profiler
4. Start the JVM
5. Execute the JVM task
6. Stop the profiler
7. Stop the JVM

Expected output:
The profiler will not be able to sample the JVM stacks correctly

Explaination:
The JVM is started after the profiling session is started so it's not attached correctly at the session start
*/
void testFlow2(int argc, char** argv) {
    CommonNonJava::loadProfiler();

    CommonNonJava::loadJvmLib();

    CommonNonJava::startProfiler();

    CommonNonJava::startJvm();

    CommonNonJava::executeJvmTask();

    CommonNonJava::stopProfiler(argv[2]);

    CommonNonJava::stopJvm();
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the profiler
4. Start the JVM
5. Execute the JVM task
6. Stop the profiler
7. Start the profiler
8. Execute the JVM task
9. Stop the profiler
10. Stop the JVM

Expected output:
The profiler will not be able to sample the JVM stacks correctly on the first session
But will be able to sample the JVM stacks correctly on the second session

Explaination:
The JVM is started after the profiling session is started so it's not attached correctly at the session start
However the second profiling session is started after the JVM is started so it's attached correctly at the session start
*/
void testFlow3(int argc, char** argv) {
    validateArgsCount(argc, 4, "Test requires 4 arguments");

    CommonNonJava::loadProfiler();

    CommonNonJava::loadJvmLib();

    CommonNonJava::startProfiler();

    CommonNonJava::startJvm();

    CommonNonJava::executeJvmTask();

    CommonNonJava::stopProfiler(argv[2]);

    CommonNonJava::startProfiler();

    CommonNonJava::executeJvmTask();

    CommonNonJava::stopProfiler(argv[3]);

    CommonNonJava::stopJvm();
}

int main(int argc, char** argv) {
    validateArgsCount(argc, 3, "Minimum Arguments is 3");

    // Check which test to run
    char* flow = argv[1];
    switch (flow[0]) {
        case '1':
            testFlow1(argc, argv);
            break;
        case '2': 
            testFlow2(argc, argv);
            break;
        case '3': 
            testFlow3(argc, argv);
            break;
        default:
            fprintf(stderr, "Unknown flow: %c\n", flow[0]);
            exit(1);
    }
    
    return 0;
}
