# Image for all tasks related to static code analysis in Async-Profiler
FROM public.ecr.aws/docker/library/amazoncorretto:11-alpine-jdk

RUN apk add --no-cache clang-extra-tools linux-headers
ENV CPLUS_INCLUDE_PATH="/usr/lib/jvm/java-11-amazon-corretto/include:/usr/lib/jvm/java-11-amazon-corretto/include/linux"

ENTRYPOINT clang-tidy $@
