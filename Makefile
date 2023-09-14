PROFILER_VERSION=3.0-ea

PACKAGE_NAME=async-profiler-$(PROFILER_VERSION)-$(OS_TAG)-$(ARCH_TAG)
PACKAGE_DIR=/tmp/$(PACKAGE_NAME)

LAUNCHER=bin/asprof
LIB_PROFILER=lib/libasyncProfiler.$(SOEXT)
API_JAR=lib/async-profiler.jar
CONVERTER_JAR=lib/converter.jar
TEST_JAR=test.jar

CFLAGS=-O3 -fno-exceptions
CXXFLAGS=-O3 -fno-exceptions -fno-omit-frame-pointer -fvisibility=hidden
INCLUDES=-I$(JAVA_HOME)/include -Isrc/helper
LIBS=-ldl -lpthread
MERGE=true

JAVAC=$(JAVA_HOME)/bin/javac
JAR=$(JAVA_HOME)/bin/jar
JAVA=$(JAVA_HOME)/bin/java
JAVA_TARGET=8
JAVAC_OPTIONS=--release $(JAVA_TARGET) -Xlint:-options

SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard src/*.h)
RESOURCES := $(wildcard src/res/*)
JAVA_HELPER_CLASSES := $(wildcard src/helper/one/profiler/*.class)
API_SOURCES := $(wildcard src/api/one/profiler/*.java)
CONVERTER_SOURCES := $(shell find src/converter -name '*.java')
TEST_SOURCES := $(shell find test -name '*.java')
TEST_DIRNAMES :=$(wildcard test/test/*/)
TESTS := $(notdir $(patsubst %/,%,$(TEST_DIRNAMES)))

ifeq ($(JAVA_HOME),)
  export JAVA_HOME:=$(shell java -cp . JavaHome)
endif

OS:=$(shell uname -s)
ifeq ($(OS),Darwin)
  CXXFLAGS += -D_XOPEN_SOURCE -D_DARWIN_C_SOURCE -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/../lib/server
  INCLUDES += -I$(JAVA_HOME)/include/darwin
  SOEXT=dylib
  PACKAGE_EXT=zip
  OS_TAG=macos
  ifeq ($(FAT_BINARY),true)
    FAT_BINARY_FLAGS=-arch x86_64 -arch arm64 -mmacos-version-min=10.12
    CFLAGS += $(FAT_BINARY_FLAGS)
    CXXFLAGS += $(FAT_BINARY_FLAGS)
    PACKAGE_NAME=async-profiler-$(PROFILER_VERSION)-$(OS_TAG)
    MERGE=false
  endif
else
  CXXFLAGS += -Wl,-z,defs
  ifeq ($(MERGE),true)
    CXXFLAGS += -fwhole-program
  endif
  LIBS += -lrt
  INCLUDES += -I$(JAVA_HOME)/include/linux
  SOEXT=so
  PACKAGE_EXT=tar.gz
  ifeq ($(findstring musl,$(shell ldd /bin/ls)),musl)
    OS_TAG=linux-musl
  else
    OS_TAG=linux
  endif
endif

ARCH:=$(shell uname -m)
ifeq ($(ARCH),x86_64)
  ARCH_TAG=x64
else
  ifeq ($(findstring arm,$(ARCH)),arm)
    ifeq ($(findstring 64,$(ARCH)),64)
      ARCH_TAG=arm64
    else
      ARCH_TAG=arm32
    endif
  else
    ifeq ($(findstring aarch64,$(ARCH)),aarch64)
      ARCH_TAG=arm64
    else
      ifeq ($(ARCH),ppc64le)
        ARCH_TAG=ppc64le
      else
        ARCH_TAG=x86
      endif
    endif
  endif
endif

ifneq ($(ARCH),ppc64le)
  ifneq ($(ARCH_TAG),arm32)
    CXXFLAGS += -momit-leaf-frame-pointer
  endif
endif


.PHONY: all release build-test test native clean

all: build/bin build/lib build/$(LIB_PROFILER) build/$(LAUNCHER) build/$(API_JAR) build/$(CONVERTER_JAR)

release: JAVA_TARGET=7

release: $(PACKAGE_NAME).$(PACKAGE_EXT)

$(PACKAGE_NAME).tar.gz: $(PACKAGE_DIR)
	tar czf $@ -C $(PACKAGE_DIR)/.. $(PACKAGE_NAME)
	rm -r $(PACKAGE_DIR)

$(PACKAGE_NAME).zip: $(PACKAGE_DIR)
	codesign -s "Developer ID" -o runtime --timestamp -v $(PACKAGE_DIR)/$(LAUNCHER) $(PACKAGE_DIR)/$(LIB_PROFILER)
	ditto -c -k --keepParent $(PACKAGE_DIR) $@
	rm -r $(PACKAGE_DIR)

$(PACKAGE_DIR): build/bin build/lib \
                build/$(LIB_PROFILER) build/$(LAUNCHER) \
                build/$(API_JAR) build/$(CONVERTER_JAR) \
                LICENSE *.md
	mkdir -p $(PACKAGE_DIR)
	cp -RP build/* LICENSE *.md $(PACKAGE_DIR)/
	chmod -R 755 $(PACKAGE_DIR)
	chmod 644 $(PACKAGE_DIR)/lib/* $(PACKAGE_DIR)/LICENSE $(PACKAGE_DIR)/*.md

build/%:
	mkdir -p $@

build/$(LAUNCHER): src/launcher/* src/jattach/* src/fdtransfer.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPROFILER_VERSION=\"$(PROFILER_VERSION)\" -DSUPPRESS_OUTPUT -o $@ src/launcher/*.cpp src/jattach/*.c
	strip $@

build/$(LIB_PROFILER): $(SOURCES) $(HEADERS) $(RESOURCES) $(JAVA_HELPER_CLASSES)
ifeq ($(MERGE),true)
	for f in src/*.cpp; do echo '#include "'$$f'"'; done |\
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DPROFILER_VERSION=\"$(PROFILER_VERSION)\" $(INCLUDES) -fPIC -shared -o $@ -xc++ - $(LIBS)
else
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DPROFILER_VERSION=\"$(PROFILER_VERSION)\" $(INCLUDES) -fPIC -shared -o $@ $(SOURCES) $(LIBS)
endif

build/$(API_JAR): $(API_SOURCES)
	mkdir -p build/api
	$(JAVAC) $(JAVAC_OPTIONS) -d build/api $(API_SOURCES)
	$(JAR) cf $@ -C build/api .
	$(RM) -r build/api

build/$(CONVERTER_JAR): $(CONVERTER_SOURCES) $(RESOURCES)
	mkdir -p build/converter
	$(JAVAC) $(JAVAC_OPTIONS) -d build/converter $(CONVERTER_SOURCES)
	$(JAR) cfe $@ Main -C build/converter . -C src/res .
	$(RM) -r build/converter

%.class: %.java
	$(JAVAC) -source $(JAVA_TARGET) -target $(JAVA_TARGET) -Xlint:-options -g:none $^

build-test: all build/$(TEST_JAR)
	echo "Successfully built all tests $(LIB_PROFILER)"

test: all build/$(TEST_JAR)
	echo "Running tests against $(LIB_PROFILER)"
	$(JAVA) -ea -cp "build/test.jar:build/lib/*" one.profiler.test.Runner $(TESTS)

build/$(TEST_JAR): $(TEST_SOURCES) build/$(CONVERTER_JAR)
	mkdir -p build/test
	$(JAVAC) --release 8 -cp "build/lib/*:build/converter/*" -d build/test $(TEST_SOURCES)
	$(JAR) cf $@ -C build/test .

native:
	mkdir -p native/linux-x64 native/linux-arm64 native/macos
	tar xfO async-profiler-$(PROFILER_VERSION)-linux-x64.tar.gz */build/libasyncProfiler.so > native/linux-x64/libasyncProfiler.so
	tar xfO async-profiler-$(PROFILER_VERSION)-linux-arm64.tar.gz */build/libasyncProfiler.so > native/linux-arm64/libasyncProfiler.so
	unzip -p async-profiler-$(PROFILER_VERSION)-macos.zip */build/libasyncProfiler.dylib > native/macos/libasyncProfiler.dylib

clean:
	$(RM) -r build
