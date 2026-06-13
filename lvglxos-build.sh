#!/bin/bash
#export BAIDU_CLIENT_ID=$(sean-token.py key baidupan AppKey)
#export BAIDU_CLIENT_SECRET=$(sean-token.py key baidupan Secretkey)
#export BAIDU_DEVICE_ID=$(sean-token.py key baidupan AppID)
#adb shell "echo export BAIDU_CLIENT_ID=${BAIDU_CLIENT_ID} >>/tmp/.env"
#adb shell "echo export BAIDU_CLIENT_SECRET=${BAIDU_CLIENT_SECRET} >>/tmp/.env"
#adb shell "echo export BAIDU_DEVICE_ID=${BAIDU_DEVICE_ID} >>/tmp/.env"
if [ "compile" == "$1" ];then
. qmenv qm10xd
SYSROOT=/home/scm/prebuilt/xos_toolchain/arm-molv2-linux-uclibcgnueabi/arm-molv2-linux-uclibcgnueabi/sysroot
export SYSROOT
unset PKG_CONFIG_PATH C_INCLUDE_PATH LIBRARY_PATH
export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig"
rm -rf build
#rm -rf build-xos
cmake -B build -DCONFIG=xosfb \
    -DCMAKE_FIND_ROOT_PATH="$SYSROOT" \
    -DCMAKE_SYSROOT="$SYSROOT" \
    -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DWAMR_BUILD_TARGET=ARM \
    -DWAMR_BUILD_INVOKE_NATIVE_GENERAL=1 \
    -DJS_APPS_DIR="/mnt/sdcard/js-app"; \
make -C build -j$(nproc);
boards/xos-xkphoto/xkphoto-runlvglsim.sh
else
boards/xos-xkphoto/xkphoto-runlvglsim.sh
fi
