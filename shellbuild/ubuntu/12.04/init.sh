#!/bin/bash

# arg1 => libname arg2 => dependency
function getLibrary()
{
    if [ $# -ne 2 ];then
        echo "should give me two args"
        exit -1
    else
        if [ ! -f $1 ];then
            echo "$1 does not exist"
            exit -1
        else
            lib=`ldd "$1" | grep "$2" | awk '{split($0,a,"=>"); print $1}'`
            loc=`ldd "$1" | grep "$2" | awk '{split($0,a,"=>"); print $3}'`
        fi
    fi
}

PREFIX=/tmp
AUTOCONF=autoconf
LIBMEM=libmemcached
CMAKE=cmake
MSGPACK=msgpack

#step 1 installing necessary packages
sudo apt-get update && sudo apt-get install -y --no-install-recommends \
    wget \
    automake \
    make \
    gcc \
    g++ \
    cmake \
    libtool \
    fakeroot \
    libssl-dev \
    memcached \
    unzip \
    curl

#step 2.1 build system initialization
wget -O "$PREFIX/$AUTOCONF.tar.gz" https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz
mkdir -p "$PREFIX/$AUTOCONF"
tar xzvf "$PREFIX/$AUTOCONF.tar.gz" -C "$PREFIX/$AUTOCONF" --strip-components=1 && cd "$PREFIX/$AUTOCONF"
./configure && make && sudo make install

#step 2.2 build libmemcached
wget -O "$PREFIX/$LIBMEM.tar.gz" https://launchpad.net/libmemcached/1.0/1.0.18/+download/libmemcached-1.0.18.tar.gz
mkdir -p "$PREFIX/$LIBMEM"
tar xzvf "$PREFIX/$LIBMEM.tar.gz" -C "$PREFIX/$LIBMEM" --strip-components=1 && cd "$PREFIX/$LIBMEM"
./configure && make && sudo make install

#step 3 download and compile msgpack locally
#first upgrade cmake firstly
wget -O "$PREFIX/$CMAKE.tar.gz" http://www.cmake.org/files/v2.8/cmake-2.8.12.1.tar.gz
mkdir -p "$PREFIX/$CMAKE"
tar xzvf "$PREFIX/$CMAKE.tar.gz" -C "$PREFIX/$CMAKE" --strip-components=1 && cd "$PREFIX/$CMAKE"
./bootstrap && make && sudo make install

#then compiling msgpack
wget -O "$PREFIX/$MSGPACK.zip" https://github.com/msgpack/msgpack-c/archive/master.zip
mkdir -p "$PREFIX/$MSGPACK"
unzip "$PREFIX/$MSGPACK.zip" -d "$PREFIX/$MSGPACK"
cd "$PREFIX/$MSGPACK/msgpack-c-master" && /usr/local/bin/cmake . && make && sudo make install

echo "Dependencies are installed and configured successfully!"
