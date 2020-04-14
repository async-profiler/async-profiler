PROFILER_VERSION=1.7
JATTACH_VERSION=1.5
JAVAC_RELEASE_VERSION=6
LIB_PROFILER=libasyncProfiler.so
JATTACH=jattach
PROFILER_JAR=async-profiler.jar
CFLAGS=-O2
CXXFLAGS=-O2
INCLUDES=-I$(JAVA_HOME)/include
LIBS=-ldl -lpthread
JAVAC=$(JAVA_HOME)/bin/javac
JAR=$(JAVA_HOME)/bin/jar
SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard src/*.h)
JAVA_SOURCES := $(wildcard src/java/one/profiler/*.java)

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

all: build build/$(LIB_PROFILER) build/$(JATTACH) build/$(PROFILER_JAR)

release: build async-profiler-$(RELEASE_TAG).tar.gz

async-profiler-$(RELEASE_TAG).tar.gz: build/$(LIB_PROFILER) build/$(JATTACH) \
                                      build/$(PROFILER_JAR) profiler.sh LICENSE NOTICE *.md
	chmod 755 build profiler.sh
	chmod 644 LICENSE NOTICE *.md
	tar cvzf $@ $^

build:
	mkdir -p build

build/$(LIB_PROFILER): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -DPROFILER_VERSION=\"$(PROFILER_VERSION)\" $(INCLUDES) -fPIC -shared -o $@ $(SOURCES) $(LIBS)

build/$(JATTACH): src/jattach/jattach.c
	$(CC) $(CFLAGS) -DJATTACH_VERSION=\"$(JATTACH_VERSION)\" -o $@ $^

build/$(PROFILER_JAR): $(JAVA_SOURCES)
	mkdir -p build/classes
	$(JAVAC) -source $(JAVAC_RELEASE_VERSION) -target $(JAVAC_RELEASE_VERSION) -d build/classes $^
	$(JAR) cvf $@ -C build/classes .
	$(RM) -r build/classes

test: all
	test/smoke-test.sh
	test/thread-smoke-test.sh
	test/alloc-smoke-test.sh
	test/load-library-test.sh
	echo "All tests passed"

clean:
	$(RM) -r build
