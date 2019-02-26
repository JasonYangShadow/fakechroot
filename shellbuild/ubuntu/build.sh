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
#step 1 installing necessary packages
sudo apt-get update && sudo apt-get install -y --no-install-recommends \
    git \
    wget \
    automake \
    make \
    gcc \
    g++ \
    cmake \
    libtool \
    fakeroot \
    libssl-dev \
    memcached

#step 2 turn off git ssl verification
git config --global http.sslVerify false

#step 2.1 build system initialization
wget -O "$PREFIX/$AUTOCONF.tar.gz" https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz
mkdir -p "$PREFIX/$AUTOCONF"
tar xzvf "$PREFIX/$AUTOCONF.tar.gz" -C "$PREFIX/$AUTOCONF" && cd "$PREFIX/$AUTOCONF"
./configure && make && make install

#step 2.2 build libmemcached
wget -O "$PREFIX/$LIBMEM.tar.gz" https://launchpad.net/libmemcached/1.0/1.0.18/+download/libmemcached-1.0.18.tar.gz
mkdir -p "$PREFIX/$LIBMEM"
tar xzvf "$PREFIX/$LIBMEM.tar.gz" -C "$PREFIX/$LIBMEM" && cd "$PREFIX/$LIBMEM"
./configure && make && make install

#step 3 download and compile msgpack locally
git clone https://github.com/msgpack/msgpack-c.git "$PREFIX"/msgpack
cd "$PREFIX"/msgpack && cmake . && make && sudo make install

#step 4 download and compile fakechroot
git clone https://github.com/JasonYangShadow/fakechroot "$PREFIX"/fakechroot
cd "$PREFIX"/fakechroot && ./autogen.sh && ./configure && make

#step 5 copy libraries
libfchroot="$PREFIX"/libfakechroot.so
cp "$PREFIX"/fakechroot/src/.libs/libfakechroot.so $libfchroot
getLibrary $libfchroot libmemcached
echo ">>> $lib $loc <<<"
if [ ! -z $lib ] && [ ! -z $loc ];then
    cp $loc "$PREFIX"/"$lib"
    libmemcached_name="$lib"
    unset lib
    unset loc
fi

getLibrary $libfchroot libsasl2
echo ">>> $lib $loc <<<"
if [ ! -z $lib ] && [ ! -z $loc ];then
    cp $loc "$PREFIX"/"$lib"
    libsasl2_name="$lib"
    unset lib
    unset loc
fi

if [ ! -f /usr/bin/memcached ];then
    echo "could not find memcached"
    exit -1
else
    mem="$PREFIX/memcached"
    cp /usr/bin/memcached $mem
    getLibrary $mem libevent
    echo ">>> $lib $loc <<<"
    if [ ! -z $lib ] && [ ! -z $loc ];then
        cp $loc "$PREFIX"/"$lib"
        libevent_name="$lib"
        unset lib
        unset loc
    fi
fi

cp /usr/lib/x86_64-linux-gnu/libfakeroot/libfakeroot-sysv.so "$PREFIX"/libfakeroot.so
cp /usr/bin/faked-sysv /tmp/faked-sysv

cd "$PREFIX"
tar czf dependency.tar.gz memcached "$libmemcached_name" "$libsasl2_name" "$libevent_name" libfakechroot.so libfakeroot.so faked-sysv
echo "DONE, All files are located in $PREFIX"
