#!/bin/bash
# lvglxos-build.sh — ARM XOSFB cross-compile build & deploy
# Usage: ./lvglxos-build.sh [compile|run|clean|-h]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-xos"
BIN="$BUILD_DIR/bin/lvglsim"
SYSROOT=/home/scm/prebuilt/xos_toolchain/arm-molv2-linux-uclibcgnueabi/arm-molv2-linux-uclibcgnueabi/sysroot

help() {
    echo "Usage: $0 <command>"
    echo ""
    echo "Commands:"
    echo "  compile   Configure & cross-compile (XOSFB, ARM)"
    echo "  deploy    Upload binary to device via adb + xkphoto script"
    echo "  run       Deploy & run on device"
    echo "  clean     Remove build directory"
    echo "  -h        Show this help"
    echo ""
    echo "Build dir: $BUILD_DIR"
    echo "Config:    XOSFB (CONFIG=xosfb)"
    echo ""
    echo "Env vars:"
    echo "  LV_ROTATION=90        Display rotation"
    echo "  BAIDU_CLIENT_ID=...   Baidu OAuth"
    exit 0
}

# Source ARM toolchain environment
setup_toolchain() {
    . qmenv qm10xd 2>/dev/null || {
        echo "ERROR: qmenv not found. Source it first."
        exit 1
    }
    export SYSROOT
    unset PKG_CONFIG_PATH C_INCLUDE_PATH LIBRARY_PATH
    export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig"
}

case "${1:-help}" in
    compile)
        setup_toolchain
        mkdir -p "$BUILD_DIR"
        cmake -B "$BUILD_DIR" -DCONFIG=xosfb -S "$SCRIPT_DIR" \
            -DCMAKE_FIND_ROOT_PATH="$SYSROOT" \
            -DCMAKE_SYSROOT="$SYSROOT" \
            -DCMAKE_SYSTEM_PROCESSOR=arm \
            -DWAMR_BUILD_TARGET=ARM \
            -DWAMR_BUILD_INVOKE_NATIVE_GENERAL=1 \
            -DJS_APPS_DIR="/mnt/sdcard/js-app"
        make -C "$BUILD_DIR" -j$(nproc) lvglsim
        echo "Build complete: $BIN"
        ;;
    deploy)
        [ -x "$BIN" ] || { echo "Binary not found: $BIN (run 'compile' first)"; exit 1; }
        "$SCRIPT_DIR/boards/xos-xkphoto/xkphoto-runlvglsim.sh"
        ;;
    run)
        [ -x "$BIN" ] || { echo "Binary not found: $BIN (run 'compile' first)"; exit 1; }
        "$SCRIPT_DIR/boards/xos-xkphoto/xkphoto-runlvglsim.sh"
        ;;
    clean)
        rm -rf "$BUILD_DIR"
        echo "Cleaned: $BUILD_DIR"
        ;;
    -h|--help|help)
        help
        ;;
    *)
        echo "Unknown command: $1"
        help
        ;;
esac
