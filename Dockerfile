# Image for building async-profiler release packages
# docker build - -t asprof <Dockerfile

# Stage 0: download musl sources and build cross-toolchains for both architectures
FROM public.ecr.aws/debian/debian:10

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        curl \
        g++ \
        libbz2-dev \
        libicu-dev \
        liblzma-dev \
        libncurses5-dev \
        libncursesw5-dev \
        libreadline-dev \
        libsqlite3-dev \
        libssl-dev \
        llvm \
        make \
        openjdk-11-jdk-headless \
        patchelf \
        sudo \
        tk-dev \
        wget \
        xz-utils \
        zlib1g-dev && \
    rm -rf /var/cache/apt /var/lib/apt/lists/*

ARG musl_src=musl-1.2.5
ARG musl_sha256=a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4

ADD https://musl.libc.org/releases/${musl_src}.tar.gz /
RUN echo ${musl_sha256} ${musl_src}.tar.gz | sha256sum -c

# Build musl.
RUN ["/bin/bash", "-c", "\
    tar xfz ${musl_src}.tar.gz && \
    cd /${musl_src} && \
    ./configure --disable-shared --prefix=/usr/local/musl && \
    make -j`nproc` && make install && make clean && \
    ln -s /usr/include/$(arch)-linux-gnu/asm /usr/include/{asm-generic,linux} /usr/local/musl/include/"]

# Build a recent python for gcovr.
ENV PATH="$PATH:/opt/python3/bin"
WORKDIR /tmp
RUN curl -fsSL https://www.python.org/ftp/python/3.13.0/Python-3.13.0.tgz | tar xzf - && cd Python-3.13.0 && \
    ./configure --prefix=/opt/python3/ --enable-optimizations && \
    make -j "$(nproc)" && make -j "$(nproc)" install && \
    cd /tmp && rm -rf /tmp/Python-*  && pip3 install gcovr

# Stage 1: install build tools + copy musl toolchain and python3 from the previous step
FROM public.ecr.aws/debian/debian:10-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        curl \
        g++ \
        libicu-dev \
        make \
        openjdk-11-jdk-headless \
        patchelf \
        sudo && \
    rm -rf /var/cache/apt /var/lib/apt/lists/*

COPY --from=0 /usr/local/musl /usr/local/musl
COPY --from=0 /opt/python3 /opt/python3

ENV PATH="$PATH:/opt/python3/bin"
