FROM public.ecr.aws/docker/library/amazoncorretto:11-alpine-jdk

RUN apk add --no-cache make g++ gdb linux-headers musl-dev util-linux patchelf gcovr bash tar
