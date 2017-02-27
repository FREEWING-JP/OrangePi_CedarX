#!/bin/bash
set -e

# ---------------------------------
# The scripts is use to auto configure CedarX envirnoment.
# Maintainer: Buddy <buddy.zhang@aliyun.com>
ROOT=`pwd`

if [ ! $ROOT = "/home/orangepi/CedarX" ]; then
    echo "$ROOT doesn't exist, Please change dirent into $ROOT"
fi

# Export dynatic shared library
export LD_LIBRARY_PATH=/home/orangepi/CedarX/arm-linux-gnueabi:/home/orangepi/CedarX/output/lib:/home/orangepi/CedarX/linuxgnueabi_3.10:/usr/arm-linux-gnueabi/lib/

# Add 32bit dynatic shared library.
ln -s /usr/arm-linux-gnueabi/lib/ld-linux.so.3 /lib/ld-linux.so.3
ln -s /home/orangepi/CedarX/arm-linux-gnueabi/libcrypto.so /home/orangepi/CedarX/arm-linux-gnueabi/libcrypto.so.1.0.0
ln -s /home/orangepi/CedarX/arm-linux-gnueabi/libz.so /home/orangepi/CedarX/arm-linux-gnueabi/libz.so.1
