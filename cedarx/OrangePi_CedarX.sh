#!/bin/bash
set -e

# ---------------------------------
# The scripts is use to auto configure CedarX envirnoment.
# Maintainer: Buddy <buddy.zhang@aliyun.com>
ROOT=`pwd`

if [ ! $ROOT = "/home/orangepi/CedarX" ]; then
    echo "$ROOT doesn't exist, Please change dirent into $ROOT"
fi

# Add 32bit dynatic shared library.
ln -s /usr/arm-linux-gnueabi/lib/ld-linux.so.3 /lib/ld-linux.so.3
ln -s /home/orangepi/CedarX/arm-linux-gnueabi/libcrypto.so /home/orangepi/CedarX/arm-linux-gnueabi/libcrypto.so.1.0.0
ln -s /home/orangepi/CedarX/arm-linux-gnueabi/libz.so /home/orangepi/CedarX/arm-linux-gnueabi/libz.so.1
