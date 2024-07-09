# Image for building async-profiler release packages for x64 and arm64

# Stage 0: download musl sources and build cross-toolchains for both architectures
FROM public.ecr.aws/lts/ubuntu:18.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo patchelf make g++ g++-aarch64-linux-gnu openjdk-11-jdk-headless && \
    rm -rf /var/cache/apt /var/lib/apt/lists/*

ARG musl_src=musl-1.2.5
ARG musl_sha256=a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4

ADD https://musl.libc.org/releases/${musl_src}.tar.gz /
RUN echo ${musl_sha256} ${musl_src}.tar.gz | sha256sum -c

RUN ["/bin/bash", "-c", "\
    tar xfz ${musl_src}.tar.gz && \
    cd /${musl_src} && \
    ./configure --disable-shared --prefix=/usr/local/musl/x86_64 && \
    make -j`nproc` && make install && make clean && \
    ./configure --disable-shared --prefix=/usr/local/musl/aarch64 --target=aarch64-linux-gnu && \
    make -j`nproc` && make install && make clean && \
    ln -s /usr/include/x86_64-linux-gnu/asm /usr/include/{asm-generic,linux} /usr/local/musl/x86_64/include/ && \
    ln -s /usr/aarch64-linux-gnu/include/{asm,asm-generic,linux} /usr/local/musl/aarch64/include/"]

# Stage 1: install build tools + copy musl toolchain from the previous step
FROM public.ecr.aws/lts/ubuntu:18.04

# This line should be exactly the same as at stage 0 to benefit from caching
RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo patchelf make g++ g++-aarch64-linux-gnu openjdk-11-jdk-headless && \
    rm -rf /var/cache/apt /var/lib/apt/lists/*

COPY --from=0 /usr/local/musl /usr/local/musl
