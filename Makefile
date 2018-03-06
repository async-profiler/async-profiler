RELEASE_TAG=1.2
LIB_PROFILER=libasyncProfiler.so
JATTACH=jattach
PROFILER_JAR=async-profiler.jar
CC=gcc
CFLAGS=-O2
CPP=g++
CPPFLAGS=-O2
INCLUDES=-I$(JAVA_HOME)/include
JAVAC=$(JAVA_HOME)/bin/javac
JAR=$(JAVA_HOME)/bin/jar

OS:=$(shell uname -s)
ifeq ($(OS), Darwin)
  CPPFLAGS += -D_XOPEN_SOURCE -D_DARWIN_C_SOURCE
  INCLUDES += -I$(JAVA_HOME)/include/darwin
  RELEASE_TAG:=$(RELEASE_TAG)-macos-x64
else
  INCLUDES += -I$(JAVA_HOME)/include/linux
  RELEASE_TAG:=$(RELEASE_TAG)-linux-x64
endif


.PHONY: all release test clean

all: build build/$(LIB_PROFILER) build/$(JATTACH) build/$(PROFILER_JAR)

release: async-profiler-$(RELEASE_TAG).zip

async-profiler-$(RELEASE_TAG).zip: build build/$(LIB_PROFILER) build/$(JATTACH) \
                                   build/$(PROFILER_JAR) profiler.sh LICENSE *.md
	zip -r $@ $^

build:
	mkdir -p build

build/$(LIB_PROFILER): src/*.cpp src/*.h
	$(CPP) $(CPPFLAGS) $(INCLUDES) -fPIC -shared -o $@ src/*.cpp -ldl -lpthread

build/$(JATTACH): src/jattach/jattach.c
	$(CC) $(CFLAGS) -o $@ $^

build/$(PROFILER_JAR): src/java/one/profiler/*.java
	mkdir -p build/classes
	$(JAVAC) -source 6 -target 6 -d build/classes $^
	$(JAR) cvf $@ -C build/classes .
	rm -rf build/classes

test: all
	test/smoke-test.sh
	test/alloc-smoke-test.sh

clean:
	rm -rf build
