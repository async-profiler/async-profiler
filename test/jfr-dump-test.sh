#!/bin/bash

set -e  # exit on any failure
# set -x  # print all executed lines

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

function assert_string() {
    if ! echo $1 | grep -q "$2"; then
        exit 1
    fi
}

# test externally called 'dump' operation
(
    echo "=== dump [jattach]"
    cd $(dirname $0)

    if [ "SleepTest.class" -ot "SleepTest.java" ]; then
        ${JAVA_HOME}/bin/javac SleepTest.java
    fi

    FILENAME=/tmp/dump_test.jfr
    DUMP_1=/tmp/dump_test_1.jfr
    DUMP_2=/tmp/dump_test_2.jfr

    rm -f $FILENAME
    rm -f $DUMP_1
    rm -f $DUMP_2

    ${JAVA_HOME}/bin/java -cp .:../build/async-profiler.jar -agentpath:../build/libasyncProfiler.so=start,wall=500ms,jfr,thread,file=$FILENAME SleepTest 3500 &
    PID=$!

    sleep 1
    ../profiler.sh $PID dump -f $DUMP_1

    sleep 2
    ../profiler.sh $PID dump -f $DUMP_2

    assert_string "$(jfr summary $DUMP_1 | grep Duration)" "Duration: 1 s"
    assert_string "$(jfr summary $DUMP_2 | grep Duration)" "Duration: 2 s"
)

# test in-process API
(
    echo "=== dump [internal API]"
    cd $(dirname $0)

    if [ "Target.class" -ot "Target.java" ]; then
        ${JAVA_HOME}/bin/javac -cp ../build/async-profiler.jar Target.java
    fi

    FILENAME=/tmp/dump_test.jfr
    DUMP_1=/tmp/dump_test_1.jfr
    DUMP_1_SECS=1
    DUMP_2=/tmp/dump_test_2.jfr
    DUMP_2_SECS=2

    rm -f $FILENAME
    rm -f $DUMP_1
    rm -f $DUMP_2

    ${JAVA_HOME}/bin/java -cp .:../build/async-profiler.jar -agentpath:../build/libasyncProfiler.so=start,wall=500ms,jfr,thread,file=$FILENAME Target $DUMP_1 $DUMP_1_SECS &
    PID=$!

    sleep $DUMP_1_SECS
    # wait for the internal dump

    sleep $DUMP_2_SECS
    if [ -z "$(${JAVA_HOME}/bin/jps | grep $PID)" ]; then
        echo "Test application died. Check the output."
        exit 1
    fi
    ../profiler.sh $PID dump -f $DUMP_2

    kill $PID

    assert_string "$(jfr summary $DUMP_1 | grep Duration)" "Duration: $DUMP_1_SECS s"
    assert_string "$(jfr summary $DUMP_2 | grep Duration)" "Duration: $DUMP_2_SECS s"
)
