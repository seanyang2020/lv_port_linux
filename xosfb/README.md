# XOSFB Library Integration

## Build libxosfb.a

Manually compile the xosfb library on the build machine:

```bash
cd /mnt/qm/xos/base/soc/qm10xd/linux/media/sample/modules/xosfb
make clean && make
```

This produces `libxosfb.a` — a self-contained static library that includes
all platform dependencies (libmpi.a, sample_comm objects, etc.).

## Install to lv_port_linux

```bash
cp /mnt/qm/xos/base/soc/qm10xd/linux/media/sample/modules/xosfb/libxosfb.a \
   /home/scm/github_gitee/lv_port_linux/xosfb/
```

The header `xosfb/include/xosfb.h` is already in place (copy from the source).

## Build lv_port_linux with XOSFB

```bash
cd /home/scm/github_gitee/lv_port_linux
./lvgl_build.sh    # uses CONFIG=xosfb
```

Or manually:

```bash
. qmenv qm10xd
SYSROOT=/home/scm/prebuilt/xos_toolchain/arm-molv2-linux-uclibcgnueabi/arm-molv2-linux-uclibcgnueabi/sysroot
export SYSROOT
cmake -B build -DCONFIG=xosfb
make -C build -j$(nproc)
```

## Runtime

```bash
# Default: ARGB8888
./build/bin/lvglsim

# Select format via env var:
XOSFB_FORMAT=ARGB1555 ./build/bin/lvglsim
XOSFB_FORMAT=ARGB0565 ./build/bin/lvglsim

# Set resolution:
LV_XOSFB_WIDTH=1024 LV_XOSFB_HEIGHT=600 ./build/bin/lvglsim

# Select original fbdev backend:
./build/bin/lvglsim -b FBDEV
```
