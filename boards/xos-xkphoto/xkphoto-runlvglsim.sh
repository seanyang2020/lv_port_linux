#!/bin/sh
adb wait-for-device
SCRIPT_DIR=$(
       	cd $(dirname $0)
       	pwd
)
# 目录推送
echo script_dir: $SCRIPT_DIR
LOCAL_DIR=$SCRIPT_DIR/xkphoto-deploy/lib
REMOTE_DIR=/mnt/sdcard/lib
adb shell mkdir -p  /mnt/sdcard/baidu-xkphoto

# 不存在远端目录则push
ret=$(adb shell "[ -d '${REMOTE_DIR}' ] && echo 1 || echo 0" | tr -d '\r')
if [ "$ret" == "0" ];then
    adb push $LOCAL_DIR $REMOTE_DIR
else
    echo $REMOTE_DIR deployed 
fi

# 脚本推送
LOCAL_SH=$SCRIPT_DIR/xkphoto-deploy/run_lvgl.sh
REMOTE_SH=/mnt/sdcard/run_lvgl.sh
ret=$(adb shell "[ -f '${REMOTE_SH}' ] && echo 1 || echo 0" | tr -d '\r')
if [ "$ret" = "0" ];then
    adb push "$LOCAL_SH" "$REMOTE_SH"
else
    echo $REMOTE_SH deployed 
fi
LVGL_LINUX_PATH=/home/scm/github_gitee/lv_port_linux
if [ ! -d $LVGL_LINUX_PATH ];then
   echo "Please config lv_port_linux realpath to LVGL_LINUX_PATH"
   echo "Try $SCRIPT_DIR as work dir"
   LVGL_LINUX_PATH=$SCRIPT_DIR
fi
alias armstrip='/home/scm/prebuilt/gcc-linaro-14.0.0-2023.06-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip'
. ~/bin/env.sh
. ~/bin/qmenv qm10xd
cp  $LVGL_LINUX_PATH/build/bin/lvglsim $LVGL_LINUX_PATH/build/bin/lvglsim_strip
armstrip $LVGL_LINUX_PATH/build/bin/lvglsim_strip
adb push $LVGL_LINUX_PATH/build/bin/lvglsim_strip /mnt/sdcard/lvglsim
export BAIDU_CLIENT_ID=$(sean-token.py key baidupan AppKey)
export BAIDU_CLIENT_SECRET=$(sean-token.py key baidupan Secretkey)
export BAIDU_DEVICE_ID=$(sean-token.py key baidupan AppID)
adb shell "echo export BAIDU_CLIENT_ID=${BAIDU_CLIENT_ID} >> /tmp/.env"
adb shell "echo export BAIDU_CLIENT_SECRET=${BAIDU_CLIENT_SECRET} >> /tmp/.env"
adb shell "echo export BAIDU_DEVICE_ID=${BAIDU_DEVICE_ID} >> /tmp/.env"
adb shell "echo export LV_ROTATION=180 >> /tmp/.env"
adb shell chmod +x /tmp/.env
adb shell pkill qxosui 
adb shell touch /tmp/.NO_BIND_WDT2QXOSUI
adb shell pkill run_lvgl.sh
adb shell pkill lvglsim
pkill -f "adb shell /mnt/sdcard/run_lvgl.sh"
adb shell /mnt/sdcard/run_lvgl.sh &
