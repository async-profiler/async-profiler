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
else
  INCLUDES += -I$(JAVA_HOME)/include/linux
endif


.PHONY: all test clean

all: build build/$(LIB_PROFILER) build/$(JATTACH)

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
