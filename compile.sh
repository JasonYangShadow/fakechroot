#!/bin/bash

make clean
./autogen.sh
./configure CFLAGS="-std=gnu99"
make
