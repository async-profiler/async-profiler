# Image for all tasks related to static code analysis in Async-Profiler
FROM public.ecr.aws/docker/library/amazoncorretto:11-alpine-jdk

RUN apk add --no-cache clang-extra-tools linux-headers make wget python3 git && \
    wget https://raw.githubusercontent.com/llvm/llvm-project/main/clang-tools-extra/clang-tidy/tool/clang-tidy-diff.py -O /usr/bin/clang-tidy-diff && \
    chmod +x /usr/bin/clang-tidy-diff
ENV CPLUS_INCLUDE_PATH="/usr/lib/jvm/java-11-amazon-corretto/include:/usr/lib/jvm/java-11-amazon-corretto/include/linux"
