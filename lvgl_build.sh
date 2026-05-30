#!/bin/bash
. qmenv qm10xd
SYSROOT=/home/scm/prebuilt/xos_toolchain/arm-molv2-linux-uclibcgnueabi/arm-molv2-linux-uclibcgnueabi/sysroot
export SYSROOT
unset PKG_CONFIG_PATH C_INCLUDE_PATH LIBRARY_PATH
export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig"
cmake -B build -DCONFIG=xosfb;make -C build -j$(nproc);
