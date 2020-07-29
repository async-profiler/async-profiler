PROFILER_VERSION=1.7.1
JATTACH_VERSION=1.5
JAVAC_RELEASE_VERSION=6
LIB_PROFILER=libasyncProfiler.so
JATTACH=jattach
API_JAR=async-profiler.jar
CONVERTER_JAR=converter.jar
CFLAGS=-O2 -fno-omit-frame-pointer
CXXFLAGS=-O2 -fno-omit-frame-pointer
INCLUDES=-I$(JAVA_HOME)/include
LIBS=-ldl -lpthread
JAVAC=$(JAVA_HOME)/bin/javac
JAR=$(JAVA_HOME)/bin/jar
SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard src/*.h)
API_SOURCES := $(wildcard src/api/one/profiler/*.java)
CONVERTER_SOURCES := $(shell find src/converter -name '*.java')

ifeq ($(JAVA_HOME),)
  export JAVA_HOME:=$(shell java -cp . JavaHome)
endif

OS:=$(shell uname -s)
ifeq ($(OS), Darwin)
  CXXFLAGS += -D_XOPEN_SOURCE -D_DARWIN_C_SOURCE
  INCLUDES += -I$(JAVA_HOME)/include/darwin
  RELEASE_TAG:=$(PROFILER_VERSION)-macos-x64
else
  LIBS += -lrt
  INCLUDES += -I$(JAVA_HOME)/include/linux
  RELEASE_TAG:=$(PROFILER_VERSION)-linux-x64
endif


.PHONY: all release test clean

all: build build/$(LIB_PROFILER) build/$(JATTACH) build/$(API_JAR) build/$(CONVERTER_JAR)

release: build async-profiler-$(RELEASE_TAG).tar.gz

async-profiler-$(RELEASE_TAG).tar.gz: build/$(LIB_PROFILER) build/$(JATTACH) \
                                      build/$(API_JAR) build/$(CONVERTER_JAR) \
                                      profiler.sh LICENSE NOTICE *.md
	chmod 755 build profiler.sh
	chmod 644 LICENSE NOTICE *.md
	tar cvzf $@ $^

build:
	mkdir -p build

build/$(LIB_PROFILER): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -DPROFILER_VERSION=\"$(PROFILER_VERSION)\" $(INCLUDES) -fPIC -shared -o $@ $(SOURCES) $(LIBS)

build/$(JATTACH): src/jattach/jattach.c
	$(CC) $(CFLAGS) -DJATTACH_VERSION=\"$(JATTACH_VERSION)\" -o $@ $^

build/$(API_JAR): $(API_SOURCES)
	mkdir -p build/api
	$(JAVAC) -source $(JAVAC_RELEASE_VERSION) -target $(JAVAC_RELEASE_VERSION) -d build/api $^
	$(JAR) cvf $@ -C build/api .
	$(RM) -r build/api

build/$(CONVERTER_JAR): $(CONVERTER_SOURCES) src/converter/MANIFEST.MF
	mkdir -p build/converter
	$(JAVAC) -source 7 -target 7 -d build/converter $(CONVERTER_SOURCES)
	$(JAR) cvfm $@ src/converter/MANIFEST.MF -C build/converter .
	$(RM) -r build/converter

test: all
	test/smoke-test.sh
	test/thread-smoke-test.sh
	test/alloc-smoke-test.sh
	test/load-library-test.sh
	echo "All tests passed"

clean:
	$(RM) -r build
