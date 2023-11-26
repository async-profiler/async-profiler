#!/bin/bash

set -e  # exit on any failure
set -x  # print all executed lines

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

(
  cd $(dirname $0)

  if [ "LoadLibraryTest.class" -ot "LoadLibraryTest.java" ]; then
     ${JAVA_HOME}/bin/javac LoadLibraryTest.java
  fi

  ${JAVA_HOME}/bin/java -agentpath:$(ls ../build/lib/libasyncProfiler.*) LoadLibraryTest &

  FILENAME=/tmp/java.trace
  JAVAPID=$!

  sleep 2     # allow the Java runtime to initialize
  ../build/bin/asprof -f $FILENAME -o collapsed -d 5 -i 1ms -L error $JAVAPID

  kill $JAVAPID

  function assert_string() {
    if ! grep -q "$1" $FILENAME; then
      exit 1
    fi
  }

  assert_string "Java_sun_management"
)
