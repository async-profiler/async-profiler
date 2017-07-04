#!/bin/bash

set -e  # exit on any failure
set -x  # print all executed lines

pushd $(dirname $0)

javac Target.java
java Target &

FILENAME=/tmp/java.trace
JAVAPID=$!

sleep 1     # allow the Java runtime to initialize
../profiler.sh -f $FILENAME -o flamegraph -d 5 $JAVAPID

kill $JAVAPID

function assert_string() {
  if ! grep -q "$1" $FILENAME; then
    exit 1
  fi
}

assert_string "Target.main;Target.method1 "
assert_string "Target.main;Target.method2 "
assert_string "Target.main;Target.method3;java/util/Scanner"

popd
