#!/bin/bash
echo "Build lvglsim as below:"
echo 'cmake -B build -DCONFIG=sdl;make -C build -j$(nproc);'

export PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:$PKG_CONFIG_PATH
LVGL_LINUX_PATH=/home/scm/github_gitee/lv_port_linux
if [ ! -d $LVGL_LINUX_PATH ];then
   echo "Please config lv_port_linux realpath to LVGL_LINUX_PATH"
   echo "Try $SCRIPT_DIR as work dir"
   LVGL_LINUX_PATH=$SCRIPT_DIR
fi
if [ "compile" == "$1" ];then
pushd $LVGL_LINUX_PATH
rm -rf build
cmake -B build -DCONFIG=sdl;make -C build -j$(nproc);
popd
unset DISPLAY
# 自动拿到 Windows 真实 IP
export DISPLAY=$(ip route show default | awk '{print $3}'):0.0
echo DISPLAY=$DISPLAY
$LVGL_LINUX_PATH/build/bin/lvglsim
else
unset DISPLAY
# 自动拿到 Windows 真实 IP
export DISPLAY=$(ip route show default | awk '{print $3}'):0.0
echo DISPLAY=$DISPLAY
$LVGL_LINUX_PATH/build/bin/lvglsim
fi


