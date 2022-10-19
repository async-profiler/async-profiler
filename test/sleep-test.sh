#!/bin/bash

set -e  # exit on any failure
set -x  # print all executed lines

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

(
  cd $(dirname $0)

  if [ "SleepTest.class" -ot "SleepTest.java" ]; then
     ${JAVA_HOME}/bin/javac SleepTest.java
  fi

  FILENAME=/tmp/java-sleep.jfr

  ${JAVA_HOME}/bin/java -cp .:../build/async-profiler.jar -agentpath:../build/libasyncProfiler.so=start,wall=500ms,jfr,thread,file=$FILENAME SleepTest 1000

  # wait for normal termination

  SAMPLES=$(jfr print --events MethodSample $FILENAME | grep SleepTest | wc -l)
  if [ $SAMPLES -ne 2 ]; then
    echo "Expected number of samples: 2, received: $SAMPLES"
    exit 1
  fi
)
