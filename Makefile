RELEASE_TAG=1.1
LIB_PROFILER=libasyncProfiler.so
JATTACH=jattach
CC=gcc
CFLAGS=-O2
CPP=g++
CPPFLAGS=-O2
INCLUDES=-I$(JAVA_HOME)/include

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

all: build build/$(LIB_PROFILER) build/$(JATTACH)

release: async-profiler-$(RELEASE_TAG).zip

async-profiler-$(RELEASE_TAG).zip: build build/$(LIB_PROFILER) build/$(JATTACH) profiler.sh LICENSE *.md
	zip -r $@ $^

build:
	mkdir -p build

build/$(LIB_PROFILER): src/*.cpp src/*.h
	$(CPP) $(CPPFLAGS) $(INCLUDES) -fPIC -shared -o $@ src/*.cpp -ldl -lpthread

build/$(JATTACH): src/jattach.c
	$(CC) $(CFLAGS) -o $@ $^

test: all
	test/smoke-test.sh
	test/alloc-smoke-test.sh

clean:
	rm -rf build
