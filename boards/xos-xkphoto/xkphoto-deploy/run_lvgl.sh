#!/bin/sh
SCRIPT_DIR=$(
        cd $(dirname $0)
        pwd
)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/sdcard/lib:$SCRIPT_DIR:$SCRIPT_DIR/lib
export PATH=$PATH:/mnt/sdcard:$SCRIPT_DIR
source /tmp/.env
echo BAIDU_CLIENT_ID=${BAIDU_CLIENT_ID}
echo BAIDU_CLIENT_SECRET=${BAIDU_CLIENT_SECRET}
echo BAIDU_DEVICE_ID=${BAIDU_DEVICE_ID}
lvglsim
