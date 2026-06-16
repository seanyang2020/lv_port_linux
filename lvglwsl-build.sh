#!/bin/bash
# lvglwsl-build.sh — WSL/PC SDL build & run
# Usage: ./lvglwsl-build.sh [compile|run|clean|-h]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-sdl"
BIN="$BUILD_DIR/bin/lvglsim"

help() {
    echo "Usage: $0 <command>"
    echo ""
    echo "Commands:"
    echo "  compile   Configure & build (SDL, x86_64)"
    echo "  run       Run the existing binary"
    echo "  clean     Remove build directory"
    echo "  -h        Show this help"
    echo ""
    echo "Build dir: $BUILD_DIR"
    echo "Config:    SDL (CONFIG=sdl)"
    exit 0
}

# Auto-detect Windows display for WSL
setup_display() {
    unset DISPLAY
    local ip=$(ip route show default 2>/dev/null | awk '{print $3}')
    if [ -n "$ip" ]; then
        export DISPLAY="${ip}:0.0"
        echo "DISPLAY=$DISPLAY"
    fi
}

case "${1:-run}" in
    compile)
        mkdir -p "$BUILD_DIR"
        export PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:$PKG_CONFIG_PATH
        cmake -B "$BUILD_DIR" -DCONFIG=sdl -S "$SCRIPT_DIR"
        make -C "$BUILD_DIR" -j$(nproc)
        echo "Build complete: $BIN"
        ;;
    run)
        [ -x "$BIN" ] || { echo "Binary not found: $BIN (run 'compile' first)"; exit 1; }
        setup_display
        exec "$BIN"
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
