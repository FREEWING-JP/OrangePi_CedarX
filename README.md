OrangePi CedarX
------------------------------------------------------

* Host Platform

  Ubuntu14.04+/x86_64

* Target Platform
  
  OrangePi PC2 - Ubuntu16.04/aarch64


### Porting Guide

* Download source code
```
git clone https://github.com/OrangePiLibra/OrangePi_Cedarx.git
```

* Prepare toolchain

  Please install toolchain on Host PC
  ```
sudo apt-get install -y gcc-arm-linux-gnueabi
  ```

* Compile cedarx on Host PC

  When we get source code of Cedarx, we should comile first, as follow:
  ```
cd OrangePi_Cedarx/cedarx
./bootstrap

./configure --host=arm-linux-gnueabi CFLAGS="-D__ENABLE_ZLIB__ \
  -DCONF_KERNEL_VERSION_3_10" CPPFLAGS="-D__ENABLE_ZLIB__ \
  -DCONF_KERNEL_VERSION_3_10" LDFLAGS="-lcrypto -lz -L~/OrangePi-Cedarx/cedarx/external/lib32/arm-linux-gnueabi -L~/OrangePi-Cedarx/libcedarc/library/lib32/linuxgnueabi_3.10" –prefix=~/OrangePi-Cedarx/cedar/

make

make install
  ```

