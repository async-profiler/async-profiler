FROM public.ecr.aws/amazonlinux/amazonlinux:2023

RUN yum update -y && \
    yum install -y binutils findutils make tar gcc-c++ gdb util-linux && \
    yum clean all && \
    rm -rf /var/cache/yum && \
    python3 -m ensurepip && \
    python3 -m pip install gcovr
