# This is a sample Dockerfile with a couple of problems.
# Paste your Dockerfile here.

FROM ubuntu:16.04
RUN apt-get update && \
    apt-get install -y --no-install-recommends\
    git \
    autoconf \
    automake \
    make \
    gcc \
    g++ \
    libmemcached-dev \
    cmake \
    libtool \
    fakeroot \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/* \

RUN mkdir /data
RUN git config --global http.sslVerify false
#step 1: compile msgpack-c
RUN git clone https://github.com/msgpack/msgpack-c.git /data/msgpack
RUN cd /data/msgpack && cmake . && make && make install

#step 2: compile fakechroot
RUN git clone https://github.com/JasonYangShadow/fakechroot /data/fakechroot
RUN cd /data/fakechroot && ./autogen.sh && ./configure && make

#step 3: copy files to shared folder
COPY /tmp/fakechroot/src/.libs/libfakechroot.so /data/libfakechroot.so
COPY /usr/lib/x86_64-linux-gnu/libfakeroot/libfakeroot-sysv.so /data/libfakeroot.so
