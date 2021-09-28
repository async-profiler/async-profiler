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

  FILENAME=/tmp/java.trace

  ${JAVA_HOME}/bin/java -agentpath:../build/libasyncProfiler.so=start,event=cpu,flat,file=$FILENAME SleepTest 1000

  # wait for normal termination

  function assert_string() {
    if ! grep -q -e "$1" $FILENAME; then
      exit 1
    fi
  }

  assert_string "Total samples[[:blank:]]\+: [0-1]" # We do not expect more than 1 sample.
)
