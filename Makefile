PROFILER_VERSION=1.6
JATTACH_VERSION=1.5
LIB_PROFILER=libasyncProfiler.so
JATTACH=jattach
PROFILER_JAR=async-profiler.jar
CC=gcc
CFLAGS=-O2
CPP=g++
CPPFLAGS=-O2
INCLUDES=-I$(JAVA_HOME)/include
LIBS=-ldl -lpthread
JAVAC=$(JAVA_HOME)/bin/javac
JAR=$(JAVA_HOME)/bin/jar

ifeq ($(JAVA_HOME),)
  export JAVA_HOME:=$(shell java -cp . JavaHome)
endif

OS:=$(shell uname -s)
ifeq ($(OS), Darwin)
  CPPFLAGS += -D_XOPEN_SOURCE -D_DARWIN_C_SOURCE
  INCLUDES += -I$(JAVA_HOME)/include/darwin
  RELEASE_TAG:=$(PROFILER_VERSION)-macos-x64
else
  LIBS += -lrt
  INCLUDES += -I$(JAVA_HOME)/include/linux
  RELEASE_TAG:=$(PROFILER_VERSION)-linux-x64
endif


.PHONY: all release test clean

all: build build/$(LIB_PROFILER) build/$(JATTACH) build/$(PROFILER_JAR)

release: build async-profiler-$(RELEASE_TAG).tar.gz

async-profiler-$(RELEASE_TAG).tar.gz: build/$(LIB_PROFILER) build/$(JATTACH) \
                                      build/$(PROFILER_JAR) profiler.sh LICENSE *.md
	chmod 755 build profiler.sh
	chmod 644 LICENSE *.md
	tar cvzf $@ $^

build:
	mkdir -p build

build/$(LIB_PROFILER): src/*.cpp src/*.h
	$(CPP) $(CPPFLAGS) -DPROFILER_VERSION=\"$(PROFILER_VERSION)\" $(INCLUDES) -fPIC -shared -o $@ src/*.cpp $(LIBS)

build/$(JATTACH): src/jattach/jattach.c
	$(CC) $(CFLAGS) -DJATTACH_VERSION=\"$(JATTACH_VERSION)\" -o $@ $^

build/$(PROFILER_JAR): src/java/one/profiler/*.java
	mkdir -p build/classes
	$(JAVAC) -source 6 -target 6 -d build/classes $^
	$(JAR) cvf $@ -C build/classes .
	rm -rf build/classes

test: all
	test/smoke-test.sh
	test/thread-smoke-test.sh
	test/alloc-smoke-test.sh
	test/load-library-test.sh
	echo "All tests passed"

clean:
	rm -rf build
