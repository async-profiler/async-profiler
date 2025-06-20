PROFILER_VERSION ?= 4.0

ifeq ($(COMMIT_TAG),true)
  PROFILER_VERSION := $(PROFILER_VERSION)-$(shell git rev-parse --short=8 HEAD)
else ifneq ($(COMMIT_TAG),)
  PROFILER_VERSION := $(PROFILER_VERSION)-$(COMMIT_TAG)
endif

TMP_DIR=/tmp
COMMA=,
PACKAGE_NAME=async-profiler-$(PROFILER_VERSION)-$(OS_TAG)-$(ARCH_TAG)
PACKAGE_DIR=$(TMP_DIR)/$(PACKAGE_NAME)
DEBUG_PACKAGE_NAME=$(PACKAGE_NAME)-debug
DEBUG_PACKAGE_DIR=$(PACKAGE_DIR)-debug

ASPROF=bin/asprof
JFRCONV=bin/jfrconv
LIB_PROFILER=lib/libasyncProfiler.$(SOEXT)
LIB_PROFILER_DEBUG=libasyncProfiler.$(SOEXT).debug
ASPROF_HEADER=include/asprof.h
API_JAR=jar/async-profiler.jar
CONVERTER_JAR=jar/jfr-converter.jar
TEST_JAR=test.jar

CC ?= gcc
CXX ?= g++
STRIP ?= strip
OBJCOPY ?= objcopy

ifneq ($(CROSS_COMPILE),)
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
STRIP := $(CROSS_COMPILE)strip
OBJCOPY := $(CROSS_COMPILE)objcopy
endif

CFLAGS_EXTRA ?=
CXXFLAGS_EXTRA ?=
CFLAGS=-O3 -fno-exceptions $(CFLAGS_EXTRA)
CXXFLAGS=-O3 -fno-exceptions -fno-omit-frame-pointer -fvisibility=hidden -std=c++11 $(CXXFLAGS_EXTRA)
CPPFLAGS=
DEFS=-DPROFILER_VERSION=\"$(PROFILER_VERSION)\"
INCLUDES=-I$(JAVA_HOME)/include -Isrc/helper
LIBS=-ldl -lpthread
MERGE=true
GCOV ?= gcov

JAVAC=$(JAVA_HOME)/bin/javac
JAR=$(JAVA_HOME)/bin/jar
JAVA=$(JAVA_HOME)/bin/java
JAVA_TARGET=8
JAVAC_OPTIONS=--release $(JAVA_TARGET) -Xlint:-options

TEST_LIB_DIR=build/test/lib
TEST_BIN_DIR=build/test/bin
LOG_DIR=build/test/logs
LOG_LEVEL=
SKIP=
TEST_FLAGS=-DlogDir=$(LOG_DIR) -DlogLevel=$(LOG_LEVEL) -Dskip='$(subst $(COMMA), ,$(SKIP))'

# always sort SOURCES so zInit is last.
SOURCES := $(sort $(wildcard src/*.cpp))
HEADERS := $(wildcard src/*.h)
RESOURCES := $(wildcard src/res/*)
JAVA_HELPER_CLASSES := $(wildcard src/helper/one/profiler/*.class)
API_SOURCES := $(wildcard src/api/one/profiler/*.java)
CONVERTER_SOURCES := $(shell find src/converter -name '*.java')
TEST_SOURCES := $(shell find test -name '*.java')
TESTS ?=
CPP_TEST_SOURCES := test/native/testRunner.cpp $(shell find test/native -name '*Test.cpp')
CPP_TEST_HEADER := test/native/testRunner.hpp
CPP_TEST_INCLUDES := -Isrc -Itest/native

ifeq ($(JAVA_HOME),)
  JAVA_HOME:=$(shell java -cp . JavaHome)
endif

OS:=$(shell uname -s)
ifeq ($(OS),Darwin)
  CXXFLAGS += -D_XOPEN_SOURCE -D_DARWIN_C_SOURCE -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/../lib/server
  INCLUDES += -I$(JAVA_HOME)/include/darwin
  SOEXT=dylib
  PACKAGE_EXT=zip
  OS_TAG=macos
  ifeq ($(FAT_BINARY),true)
    FAT_BINARY_FLAGS=-arch x86_64 -arch arm64 -mmacos-version-min=10.15
    CFLAGS += $(FAT_BINARY_FLAGS)
    CXXFLAGS += $(FAT_BINARY_FLAGS)
    PACKAGE_NAME=async-profiler-$(PROFILER_VERSION)-$(OS_TAG)
    MERGE=false
  endif
  SHA256SUM=shasum -a 256
else
  CXXFLAGS += -U_FORTIFY_SOURCE -Wl,-z,defs -Wl,--exclude-libs,ALL -static-libstdc++ -static-libgcc -fdata-sections -ffunction-sections -Wl,--gc-sections -ggdb
  ifeq ($(MERGE),true)
    CXXFLAGS += -fwhole-program
  endif
  LIBS += -lrt
  INCLUDES += -I$(JAVA_HOME)/include/linux
  SOEXT=so
  PACKAGE_EXT=tar.gz
  OS_TAG=linux
  SHA256SUM=sha256sum
endif

ifeq ($(ARCH_TAG),)
  ARCH:=$(shell uname -m)
  ifeq ($(ARCH),x86_64)
    ARCH_TAG=x64
  else ifeq ($(ARCH),aarch64)
    ARCH_TAG=arm64
  else ifeq ($(ARCH),arm64)
    ARCH_TAG=arm64
  else ifeq ($(findstring arm,$(ARCH)),arm)
    ARCH_TAG=arm32
  else ifeq ($(ARCH),ppc64le)
    ARCH_TAG=ppc64le
  else ifeq ($(ARCH),riscv64)
    ARCH_TAG=riscv64
  else ifeq ($(ARCH),loongarch64)
    ARCH_TAG=loongarch64
  else
    ARCH_TAG=x86
  endif
endif

STATIC_BINARY=$(findstring musl-gcc,$(CC))
ifneq (,$(STATIC_BINARY))
  CFLAGS += -static -fdata-sections -ffunction-sections -Wl,--gc-sections
endif

ifneq (,$(findstring $(ARCH_TAG),x86 x64 arm64))
  CXXFLAGS += -momit-leaf-frame-pointer
endif

# OTLP Protobuf
OTEL_PROTO_PATH=$(TMP_DIR)/opentelemetry-proto
OTEL_PROTO_JAR=opentelemetry-gen-classes.jar
PB_JAVA_VERSION=4.31.1
PB_JAVA_JAR_SHA256=d60dfe7c68a0d38a248cca96924f289dc7e1966a887ee7cae397701af08575ae
PB_JAVA_JAR_URL=https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java/$(PB_JAVA_VERSION)/protobuf-java-$(PB_JAVA_VERSION).jar
PB_JAVA_JAR=protobuf-java.jar
PB_BIN_PATH=$(TMP_DIR)/protobuf
PB_BIN_VERSION=31.1
PB_BIN_URL=https://github.com/protocolbuffers/protobuf/releases/download/v$(PB_BIN_VERSION)/protoc-$(PB_BIN_VERSION)-linux-$(ARCH).zip
TEST_DEPS_DIR=test/deps
TEST_GEN_DIR=test/gen

.PHONY: all jar release build-test test clean coverage clean-coverage build-test-java build-test-cpp build-test-libs build-test-bins test-cpp test-java check-md format-md

all: build/bin build/lib build/$(LIB_PROFILER) build/$(ASPROF) jar build/$(JFRCONV) build/$(ASPROF_HEADER)

jar: build/jar build/$(API_JAR) build/$(CONVERTER_JAR)

release: $(PACKAGE_NAME).$(PACKAGE_EXT)

$(PACKAGE_NAME).tar.gz: $(PACKAGE_DIR)
	patchelf --remove-needed ld-linux-x86-64.so.2 --remove-needed ld-linux-aarch64.so.1 $(PACKAGE_DIR)/$(LIB_PROFILER)
	tar czf $@ -C $(PACKAGE_DIR)/.. $(PACKAGE_NAME)
	rm -r $(PACKAGE_DIR)

	tar czf $(DEBUG_PACKAGE_NAME).tar.gz -C $(DEBUG_PACKAGE_DIR)/.. $(DEBUG_PACKAGE_NAME)
	rm -r $(DEBUG_PACKAGE_DIR)

$(PACKAGE_NAME).zip: $(PACKAGE_DIR)
	truncate -cs -`stat -f "%z" build/$(CONVERTER_JAR)` $(PACKAGE_DIR)/$(JFRCONV)
ifneq ($(GITHUB_ACTIONS), true)
	codesign -s "Developer ID" -o runtime --timestamp -v $(PACKAGE_DIR)/$(ASPROF) $(PACKAGE_DIR)/$(JFRCONV) $(PACKAGE_DIR)/$(LIB_PROFILER)
endif
	cat build/$(CONVERTER_JAR) >> $(PACKAGE_DIR)/$(JFRCONV)
	ditto -c -k --keepParent $(PACKAGE_DIR) $@
	rm -r $(PACKAGE_DIR)

$(PACKAGE_DIR): all LICENSE README.md
	rm -rf $@
	mkdir -p $(PACKAGE_DIR) $(DEBUG_PACKAGE_DIR)
	cp -RP build/bin build/lib build/include LICENSE README.md $(PACKAGE_DIR)/
	chmod -R 755 $(PACKAGE_DIR)
	chmod 644 $(PACKAGE_DIR)/lib/* $(PACKAGE_DIR)/include/* $(PACKAGE_DIR)/LICENSE $(PACKAGE_DIR)/README.md

ifeq ($(OS_TAG),linux)
	$(STRIP) --only-keep-debug build/$(LIB_PROFILER) -o $(DEBUG_PACKAGE_DIR)/$(LIB_PROFILER_DEBUG)
	$(STRIP) -g $@/$(LIB_PROFILER)
	$(OBJCOPY) --add-gnu-debuglink=$(DEBUG_PACKAGE_DIR)/$(LIB_PROFILER_DEBUG) $@/$(LIB_PROFILER)
	chmod 644 $(DEBUG_PACKAGE_DIR)/*
endif

build/%:
	mkdir -p $@

build/$(ASPROF): src/main/* src/jattach/* src/fdtransfer.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFS) -o $@ src/main/*.cpp src/jattach/*.c
	$(STRIP) $@

build/$(JFRCONV): src/launcher/* build/$(CONVERTER_JAR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFS) -o $@ src/launcher/*.cpp
	$(STRIP) $@
	cat build/$(CONVERTER_JAR) >> $@

build/$(LIB_PROFILER): $(SOURCES) $(HEADERS) $(RESOURCES) $(JAVA_HELPER_CLASSES)
ifeq ($(MERGE),true)
	for f in src/*.cpp; do echo '#include "'$$f'"'; done |\
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEFS) $(INCLUDES) -fPIC -shared -o $@ -xc++ - $(LIBS)
else
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEFS) $(INCLUDES) -fPIC -shared -o $@ $(SOURCES) $(LIBS)
endif

build/$(ASPROF_HEADER): src/asprof.h
	mkdir -p build/include
	cp -f $< build/include

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
	$(JAVAC) -source 7 -target 7 -Xlint:-options -g:none $^

build/test/cpptests: $(CPP_TEST_SOURCES) $(CPP_TEST_HEADER) $(SOURCES) $(HEADERS) $(RESOURCES) $(JAVA_HELPER_CLASSES)
	mkdir -p build/test
ifeq ($(MERGE),true)
	for f in src/*.cpp test/native/*.cpp; do echo '#include "'$$f'"'; done |\
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEFS) $(INCLUDES) $(CPP_TEST_INCLUDES) -fPIC -o $@ -xc++ - $(LIBS)
else
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEFS) $(INCLUDES) $(CPP_TEST_INCLUDES) -fPIC -o $@ $(SOURCES) $(CPP_TEST_SOURCES) $(LIBS)
endif

build-test-java: all build/$(TEST_JAR) build-test-libs build-test-bins

build-test-cpp: build/test/cpptests build-test-libs

build-test: build-test-cpp build-test-java

build-test-libs:
	@mkdir -p $(TEST_LIB_DIR)
	$(CC) -shared -fPIC -o $(TEST_LIB_DIR)/libreladyn.$(SOEXT) test/native/libs/reladyn.c
	$(CC) -shared -fPIC -o $(TEST_LIB_DIR)/libcallsmalloc.$(SOEXT) test/native/libs/callsmalloc.c
	$(CC) -shared -fPIC $(INCLUDES) -Isrc -o $(TEST_LIB_DIR)/libjnimalloc.$(SOEXT) test/native/libs/jnimalloc.c
	$(CC) -shared -fPIC -o $(TEST_LIB_DIR)/libmalloc.$(SOEXT) test/native/libs/malloc.c
	$(CC) -fno-optimize-sibling-calls -shared -fPIC $(INCLUDES) -Isrc -o $(TEST_LIB_DIR)/libjninativestacks.$(SOEXT) test/native/libs/jninativestacks.c

ifeq ($(OS_TAG),linux)
	$(CC) -c -shared -fPIC -o $(TEST_LIB_DIR)/vaddrdif.o test/native/libs/vaddrdif.c
	$(LD) -N -shared -o $(TEST_LIB_DIR)/libvaddrdif.$(SOEXT) $(TEST_LIB_DIR)/vaddrdif.o -T test/native/libs/vaddrdif.ld

	$(AS) -o $(TEST_LIB_DIR)/multiplematching.o test/native/libs/multiplematching.s
	$(LD) -shared -o $(TEST_LIB_DIR)/multiplematching.$(SOEXT) $(TEST_LIB_DIR)/multiplematching.o

	$(AS) -o $(TEST_LIB_DIR)/twiceatzero.o test/native/libs/twiceatzero.s
	$(LD) -shared -o $(TEST_LIB_DIR)/libtwiceatzero.$(SOEXT) $(TEST_LIB_DIR)/twiceatzero.o --section-start=.seg1=0x4000 -z max-page-size=0x1000
endif

build-test-bins:
	@mkdir -p $(TEST_BIN_DIR)
	$(CC) -o $(TEST_BIN_DIR)/malloc_plt_dyn test/test/nativemem/malloc_plt_dyn.c
	$(CC) -o $(TEST_BIN_DIR)/native_api -Isrc test/test/c/native_api.c -ldl
	$(CC) -o $(TEST_BIN_DIR)/profile_with_dlopen -Isrc test/test/nativemem/profile_with_dlopen.c -ldl
	$(CC) -o $(TEST_BIN_DIR)/preload_malloc -Isrc test/test/nativemem/preload_malloc.c -ldl
	$(CC) -o $(TEST_BIN_DIR)/nativemem_known_lib_crash -Isrc test/test/nativemem/nativemem_known_lib_crash.c -ldl
	$(CXX) -o $(TEST_BIN_DIR)/non_java_app -std=c++11 $(INCLUDES) $(CPP_TEST_INCLUDES) test/test/nonjava/non_java_app.cpp $(LIBS)

test-cpp: build-test-cpp
	echo "Running cpp tests..."
	LD_LIBRARY_PATH="$(TEST_LIB_DIR)" DYLD_LIBRARY_PATH="$(TEST_LIB_DIR)" build/test/cpptests

test-java: build-test-java
	echo "Running tests against $(LIB_PROFILER)"
	$(JAVA) "-Djava.library.path=$(TEST_LIB_DIR)" $(TEST_FLAGS) -ea -cp "build/$(TEST_JAR):build/jar/*:build/lib/*:$(TEST_DEPS_DIR)/*:$(TEST_GEN_DIR)/*" one.profiler.test.Runner $(subst $(COMMA), ,$(TESTS))

coverage: override FAT_BINARY=false
coverage: clean-coverage
	$(MAKE) test-cpp CXXFLAGS_EXTRA="-fprofile-arcs -ftest-coverage -fPIC -O0 --coverage"
	mkdir -p build/test/coverage
	cd build/test/ && gcovr -r ../.. --html-details --gcov-executable "$(GCOV)" -o coverage/index.html
	rm -rf -- -.gc*

test: test-cpp test-java

$(TEST_DEPS_DIR): $(TEST_DEPS_DIR)/$(PB_JAVA_JAR)

build/$(TEST_JAR): build/$(API_JAR) $(TEST_SOURCES) build/$(CONVERTER_JAR) $(TEST_DEPS_DIR)
	mkdir -p build/test
	$(JAVAC) -source $(JAVA_TARGET) -target $(JAVA_TARGET) -Xlint:-options -cp "build/jar/*:$(TEST_DEPS_DIR)/*" -d build/test $(TEST_SOURCES)
	$(JAR) cf $@ -C build/test .

check-md:
	prettier -c README.md "docs/**/*.md"

format-md:
	prettier -w README.md "docs/**/*.md"

clean-coverage:
	$(RM) -rf build/test/cpptests build/test/coverage

clean:
	$(RM) -r build

$(TEST_DEPS_DIR)/$(PB_JAVA_JAR): Makefile
	mkdir -p $(TEST_DEPS_DIR)
	curl -o $(TEST_DEPS_DIR)/$(PB_JAVA_JAR) $(PB_JAVA_JAR_URL)
	echo "$(PB_JAVA_JAR_SHA256)  $(TEST_DEPS_DIR)/$(PB_JAVA_JAR)" | $(SHA256SUM) -c

update-otlp-classes-jar: $(TEST_DEPS_DIR)/$(PB_JAVA_JAR)
	$(RM) -rf $(OTEL_PROTO_PATH)
	git clone --depth 1 git@github.com:open-telemetry/opentelemetry-proto.git $(OTEL_PROTO_PATH)
	# Classes related to logs, metrics and traces are not needed
	find $(OTEL_PROTO_PATH)/opentelemetry/proto/ \
		-type f \( -name 'logs*.proto' -o -name 'metrics*.proto' -o -name 'trace*.proto' -o -name '*service.proto' \) \
		-delete

	$(RM) -rf $(PB_BIN_PATH) $(PB_BIN_PATH).zip
	curl -L -o $(PB_BIN_PATH).zip $(PB_BIN_URL)
	unzip $(PB_BIN_PATH).zip -d $(PB_BIN_PATH)

	mkdir -p $(OTEL_PROTO_PATH)/gen/java
	cd $(OTEL_PROTO_PATH) && $(PB_BIN_PATH)/bin/protoc --java_out=./gen/java $$(find . -name "*.proto")
	$(JAVAC) -source $(JAVA_TARGET) \
	    -target $(JAVA_TARGET) \
	    -cp $(TEST_DEPS_DIR)/$(PB_JAVA_JAR) \
	    -d $(OTEL_PROTO_PATH)/build \
	    $$(find $(OTEL_PROTO_PATH)/gen/java -name "*.java" | tr '\n' ' ')
	$(JAR) cvf $(TEST_GEN_DIR)/$(OTEL_PROTO_JAR) -C $(OTEL_PROTO_PATH)/build .
