#!/bin/bash

set -e  # exit on any failure
set -x  # print all executed lines

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

(
  cd $(dirname $0)

  if [ "Target.class" -ot "Target.java" ]; then
     ${JAVA_HOME}/bin/javac -cp ../build/async-profiler.jar Target.java
  fi

  ${JAVA_HOME}/bin/java Target &

  FILENAME=/tmp/java-smoke.trace
  JAVAPID=$!

  function cleanup {
    kill $JAVAPID
  }

  trap cleanup EXIT

  sleep 1     # allow the Java runtime to initialize
  ../profiler.sh -f $FILENAME -o collapsed -d 5 $JAVAPID

  function assert_string() {
    if ! grep -q "$1" $FILENAME; then
      exit 1
    fi
  }

  assert_string "Target.main;Target.method1 "
  assert_string "Target.main;Target.method2 "
  assert_string "Target.main;Target.method3;java/io/File"
)
