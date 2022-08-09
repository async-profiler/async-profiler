#! /bin/bash
set -euo pipefail

export PROFILER_VERSION=$1
FORCE_TESTS=$2
FORCE_CPPCHECK=$3

cd /data/src/async-profiler
# make sure all previously compiled classes are gone to prevent any bytecode incompatibility
find . -name '*.class' | xargs -I {} rm -f {} || true
# build the library

# set up Java home according to the actual OS
if [ -d /usr/lib/jvm/java-11-openjdk-amd64/ ]; then
  export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64/
elif [ -d /usr/lib/jvm/java-11-openjdk/ ]; then
  export JAVA_HOME=/usr/lib/jvm/java-11-openjdk/
else
  echo "Unsupported Java home"
  exit 1
fi

make clean all
if [ "yes" = "$FORCE_CPPCHECK" ]; then
  make cppcheck
fi
if [ "yes" = "$FORCE_TESTS" ]; then
  make test
fi
cp build/libasyncProfiler.so /data/libs/libasyncProfiler.so