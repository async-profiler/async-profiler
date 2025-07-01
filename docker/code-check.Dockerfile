# Image for all tasks related to static code analysis in async-profiler
FROM public.ecr.aws/docker/library/amazoncorretto:11-alpine-jdk

ADD --chmod=555 https://raw.githubusercontent.com/llvm/llvm-project/67be4fe3d5fd986a3149de3806bcf2c92320015e/clang-tools-extra/clang-tidy/tool/clang-tidy-diff.py /usr/bin/
RUN apk add --no-cache clang-extra-tools linux-headers make python3 git && \
    chmod +x /usr/bin/clang-tidy-diff.py
ENV CPLUS_INCLUDE_PATH="/usr/lib/jvm/java-11-amazon-corretto/include:/usr/lib/jvm/java-11-amazon-corretto/include/linux"
