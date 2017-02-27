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

./configure --host=arm-linux-gnueabi  CFLAGS="-D__ENABLE_ZLIB__" CPPFLAGS="-D__ENABLE_ZLIB__" LDFLAGS="-lcrypto -lz -L${CURRENT_PATH}/OrangePi_CedarX/cedarx/external/lib32/arm-linux-gnueabi -L${CURRENT_PATH}/OrangePi_CedarX/libcedarc/library/lib32/linuxgnueabi_3.10/" --prefix=${CURRENT_PATH}/OrangePi_CedarX/output

make

make install
  ```
  Note! The "CURRENT_PATH" is path of "OrangePi_CedarX"

  Now, we can get executable file on "${CURRENT_PATH}/OrangePi_CedarX/output".

* Update executable file onto OrangePi PC2

  Copy executable file  to OrangePi PC2.
  ```
cp -rfa ${CURRENT_PATH}/OrangePi_CedarX/output ${OrangePiPC2}/rootfs/home/orangepi/CedarX
  ```
  Copy 32bit-library of "arm-linux-gnueabi" to OrangePi PC2.
  ```
cp -rfa ${CURRENT_PATH}/OrangePi_CedarX/lib32/arm-linux-gnueabi ${OrangePiPC2}/rootfs/home/orangepi/CedarX
  ```
  Copy 32bit-library of "linuxgnuebi" to OrangePi PC2.
  ```
cp -rfa ${CURRENT_PATH}/OrangePi_CedarX/libcedarc/library/lib32/linuxgnueabi_3.10/ ${OrangePiPC2}/rootfs/home/orangepi/CedarX
  ```
  Copy specify file to OrangePi PC2.
  ```
cp -rfa ${CURRENT_PATH}/OrangePi_CedarX/cedarx/config/t3_linux_cedarx.conf ${OrangePiPC2}/rootfs/etc/cedarx.conf
  ```
