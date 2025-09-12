# Image for all tasks related to static code analysis in async-profiler
FROM public.ecr.aws/docker/library/amazoncorretto:11-alpine-jdk

RUN apk update && \
    apk add \
      linux-headers \
      g++ \
      libc++ \
      libc++-dev \
      make \
      git \
      python3 \
      py3-pip \
      bash

RUN apk add \
    --repository=https://dl-cdn.alpinelinux.org/alpine/edge/main \
    clang21-extra-tools
ADD --chmod=555 https://raw.githubusercontent.com/llvm/llvm-project/5374f16270f02fdbedbbba96951a8b9ffd9c482f/clang-tools-extra/clang-tidy/tool/clang-tidy-diff.py /usr/bin/
ENV PATH="/usr/lib/llvm21/bin:$PATH"

# Needed by clang-tidy-diff.py to merge multiple results in one file.
# '--break-system-packages' is needed because Alpine does not like other package managers than 'apk' ('pip' in this case) to install
# software globally, but it's safe to do in this case.
RUN pip install --break-system-packages pyyaml
ENV CPLUS_INCLUDE_PATH="/usr/lib/jvm/java-11-amazon-corretto/include:/usr/lib/jvm/java-11-amazon-corretto/include/linux"

RUN rm -rf /var/cache/apk/*
