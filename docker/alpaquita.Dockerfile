FROM public.ecr.aws/bellsoft/alpaquita-linux-gcc:15.2-musl

RUN apk add --no-cache liberica21-jdk util-linux-misc curl
