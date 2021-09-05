#!/bin/bash
# based on smoke-test.sh

set -e  # exit on any failure
set -x  # print all executed lines

UNAME_S=$(uname -s)
if [ "$UNAME_S" != "Linux" ]; then
  # This test is Linux-specific; do nothing on other platforms
  exit 0
fi

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

(
  cd $(dirname $0)

  if [ "Target.class" -ot "Target.java" ]; then
     ${JAVA_HOME}/bin/javac Target.java
  fi

  ${JAVA_HOME}/bin/java Target &

  FILENAME=/tmp/java.trace
  JAVAPID=$!

  sleep 1     # allow the Java runtime to initialize
  sudo ../profiler.sh -f $FILENAME -o collapsed -e cpu -i 1ms --fdtransfer -d 5 $JAVAPID

  kill $JAVAPID

  function assert_string() {
    if ! grep -q "$1" $FILENAME; then
      exit 1
    fi
  }

  assert_string "Target.main;Target.method1 "
  assert_string "Target.main;Target.method2 "
  assert_string "Target.main;Target.method3;java/io/File"
  assert_string "sys_getdents"
)
