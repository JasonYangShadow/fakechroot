# This is a sample Dockerfile with a couple of problems.
# Paste your Dockerfile here.

FROM ubuntu:16.04
VOLUME /tmp/ubuntu:16.04 /data
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
    && rm -rf /var/lib/apt/lists/* \

#step 1: compile msgpack-c
RUN git clone https://github.com/msgpack/msgpack-c.git /tmp && cd /tmp/msgpack-c && cmake . && make && make install

#step 2: compile fakechroot
RUN git clone https://github.com/JasonYangShadow/fakechroot /tmp && cd /tmp/fakechroot && ./autogen.sh && ./configure && make

#step 3: copy files to shared folder
COPY /tmp/fakechroot/src/.libs/libfakechroot.so /data
