#!/bin/bash

os=`cat /etc/os-release | grep -w NAME= | cut -c 6-`
if [ "$os" == "\"Ubuntu\"" ] || [ "$os" == "\"Debian GNU/Linux\"" ]
then
    sudo apt-get update && sudo apt-get install -y --no-install-recommends \
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
        libssl-dev

    git config --global http.sslVerify false
    git clone https://github.com/msgpack/msgpack-c.git /tmp/msgpack
    cd /tmp/msgpack && cmake . && make && sudo make install

    git clone https://github.com/JasonYangShadow/fakechroot /tmp/fakechroot
    cd /tmp/fakechroot && ./autogen.sh && ./configure CFLAGS="-std=c99" && make

    cp /tmp/fakechroot/src/.libs/libfakechroot.so /tmp/libfakechroot.so
    cp /usr/lib/x86_64-linux-gnu/libfakeroot/libfakeroot-sysv.so /tmp/libfakeroot.so
    cp /usr/lib/x86_64-linux-gnu/libmemcached.so.10 /tmp/libmemcached.so.10
    cp /usr/lib/x86_64-linux-gnu/libsasl2.so.2 /tmp/libsasl2.so.2
    cp /usr/lib/x86_64-linux-gnu/libevent-2.0.so.5 /tmp/libevent.so
    cp /usr/bin/faked-sysv /tmp/faked-sysv
else 
    echo "Your system type is $os, this script is used for 'ubuntu' or 'debian' like systems"
    exit 1
fi
