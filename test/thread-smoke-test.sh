#!/bin/bash

set -e  # exit on any failure
set -x  # print all executed lines

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

(
  cd $(dirname $0)

  if [ "ThreadsTarget.class" -ot "ThreadsTarget.java" ]; then
     ${JAVA_HOME}/bin/javac ThreadsTarget.java
  fi

  FILENAME=/tmp/java.trace

  ${JAVA_HOME}/bin/java -agentpath:../build/libasyncProfiler.so=start,event=cpu,collapsed,threads,file=$FILENAME ThreadsTarget

  # wait for normal termination

  function assert_string() {
    if ! grep -q "$1" $FILENAME; then
      exit 1
    fi
  }

  assert_string "\[ThreadEarlyEnd tid=[0-9]\+\];.*ThreadsTarget.methodForThreadEarlyEnd;.*"
  assert_string "\[RenamedThread tid=[0-9]\+\];.*ThreadsTarget.methodForRenamedThread;.*"
)
