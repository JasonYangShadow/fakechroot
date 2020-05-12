#!/bin/bash

NAME=centos-5.7
#arg1 => libname arg2 =>dependency

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

PREFIX=~/compile
FAKECHROOT=fakechroot

#clean previous build
rm -rf "$PREFIX"
mkdir -p "$PREFIX"

#step1 download and compile fakechroot
#in centos 5.5 we could not establish ssl connection, but could use /vagrant share folder, so please exec the following command outside of virtualbox and copy the package to /vagrant share folder
if [ -f /vagrant/fakechroot.zip ];then
    mkdir -p "$PREFIX/$FAKECHROOT"
    unzip "/vagrant/$FAKECHROOT.zip" -d "$PREFIX/$FAKECHROOT"
    export CC=/usr/bin/gcc44
    export CXX=/usr/bin/g++44
    cd "$PREFIX/$FAKECHROOT/fakechroot-master" && ./autogen.sh && ./configure CFLAGS="-std=gnu99" && make
else
    "please download 'http://github.com/JasonYangShadow/fakechroot/archive/master.zip' and name it to fakechroot.zip then put it inside /vagrant share folder"
    exit -1
fi

#step2 copy libraries
libfchroot="$PREFIX"/libfakechroot.so
cp "$PREFIX/$FAKECHROOT/fakechroot-master"/src/.libs/libfakechroot.so $libfchroot

if [ -f "/usr/local/lib/libmemcached.so.11" ];then
    cp /usr/local/lib/libmemcached.so.11 "$PREFIX"/libmemcached.so.11
fi

if [ -f /usr/local/bin/memcached ];then
    mem="$PREFIX/memcached"
    cp /usr/local/bin/memcached $mem
    getLibrary $mem libevent
    if [ ! -z $lib ] && [ ! -z $loc ];then
        cp $loc "$PREFIX/libevent.so"
        libevent_name="libevent.so"
        unset lib
        unset loc
    fi
fi

LIBSYS="/usr/lib64/libfakeroot/libfakeroot-sysv.so"
if [ -f "$LIBSYS" ];then
    cp "$LIBSYS" "$PREFIX"/libfakeroot.so
fi

if [ -f /usr/bin/faked-sysv ];then
    cp /usr/bin/faked-sysv "$PREFIX"/faked-sysv
fi

if [ -f "$PREFIX/libmemcached.so.11" ] && [ -f "$PREFIX/libevent.so" ] && [ -f "$PREFIX/libfakechroot.so" ] && [ -f "$PREFIX/libfakeroot.so" ] && [ -f "$PREFIX/faked-sysv" ]; then
    cd "$PREFIX"
    tar czf dependency.tar.gz memcached libmemcached.so.11 libevent.so libfakechroot.so libfakeroot.so faked-sysv
    cp "$PREFIX/dependency.tar.gz" /vagrant
else
    echo "could not find necessary dependencies, something goes wrong"
    exit -1
fi
