![Build Status](https://travis-ci.com/JasonYangShadow/fakechroot.svg?branch=master)
# What is it?

fakechroot runs a command in an environment were is additional possibility to
use `chroot`(8) command without root privileges.  This is useful for allowing
users to create own chrooted environment with possibility to install another
packages without need for root privileges.


# This is a customized fakechroot for [LPMX](https://github.com/jasonyangshadow/lpmx)

We add many features to the original fakechroot, including fake union file system implementation, dynamically privileges management and more. 

Generally, to make it easy to compile everything, there are build scripts [HERE](https://github.com/JasonYangShadow/fakechroot/tree/master/shellbuild) for different distros to build all dependencies required by [LPMX Project](https://github.com/jasonyangshadow/lpmx).

If you would like to compile everything from scratch and clearly understand why you need to build it, you might need at least the following dependencies and patient:

1. git
2. autoconf(some old distros will fail to compile source code because of older autoconf(should be >2.64), in this case, please download newer autoconf source code [HERE](https://ftp.gnu.org/gnu/autoconf/) and compile it locally)
3. automake
4. make
5. gcc
6. g++
7. libmemcached-dev(also may need to download source code [HERE](https://launchpad.net/libmemcached/+download) to compile locally)
8. cmake
9. libtool

Fakechroot build script will automatically check the building tools version, if any prerequisites are not satisfied, try downloading and compiling their source code locally.

If you could directly install msgpack-c via your package manager, it will be good and you don't need cmake. For example, for arch linux, one could directly install msgpack-c package from AUR by executing 'yaourt -S msgpack-c'. For other distros, such as ubuntu, you may need to compile msgpack-c from source by following the steps:

```
git clone https://github.com/msgpack/msgpack-c.git
cd msgpack-c
cmake .
make
sudo make install
```

After these steps, you could start compiling fakechroot.

```
git clone https://github.com/JasonYangShadow/fakechroot
cd fakechroot
./autogen.sh
./configure
make
```

Done! Congratulations!

**Dependencies required by LPMX are libfakechroot, libfakeroot, faked-sys, libevent, libmemcached, libsasl2, memcached**
