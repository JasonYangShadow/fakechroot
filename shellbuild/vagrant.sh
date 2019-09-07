#!/bin/bash

DISTROS=(jasonyangshadow/ubuntu-19.04 jasonyangshadow/ubuntu-18.04 jasonyangshadow/ubuntu-16.04 jasonyangshadow/ubuntu-14.04 jasonyangshadow/ubuntu-12.04 jasonyangshadow/centos-6 jasonyangshadow/cent-6.7 jasonyangshadow/centos-7 jasonyangshadow/centos-5.5)
TARGETDIR=`pwd`/LPMXBuild
GITHUB=https://raw.githubusercontent.com/JasonYangShadow/fakechroot/master/shellbuild/

function downloadBox(){
    RET=`vagrant box list | grep "$1"`
    if [ -z "$RET" ];then
        echo "could not find $1, let me download it firstly"
        vagrant box add "$1"
    fi
}

function run(){
    mkdir -p "$TARGETDIR" && cd "$TARGETDIR"
    for item in "${DISTROS[@]}";
    do
        NAME=`echo "$item" | awk -F"/" '{print $2}'`
        echo "current job is compiling lpmx's dependencies for platform: $NAME"
        mkdir -p "$TARGETDIR/$NAME" && cd "$TARGETDIR/$NAME"
        N1=`echo "$NAME" | awk -F"-" '{print $1}'`
        N2=`echo "$NAME" | awk -F"-" '{print $2}'`
        # get compile.sh and Vagrantfile
        if [ ! -f compile.sh ];then
            wget -O compile.sh "$GITHUB$N1/$N2/compile.sh"
        fi
        if [ ! -f Vagrantfile ]; then
            wget -O Vagrantfile "$GITHUB$N1/$N2/Vagrantfile_run"
        fi
        # init
        vagrant up
        OUTPUT=`vagrant provision | tail -n 1`
        echo "$NAME job done, the output is:$OUTPUT"
        vagrant halt
    done
}

if [ -f /usr/bin/vagrant ];then
    for item in "${DISTROS[@]}";
        # check and download images
    do
        downloadBox "$item"
    done
    run
else
    echo "could not locate /usr/bin/vagrant"
    exit -1
fi
