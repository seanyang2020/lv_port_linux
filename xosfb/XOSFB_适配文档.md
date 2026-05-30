# XOSFB — qm10xd LVGL Framebuffer 适配文档

## 概述

`libxosfb.a` 是针对 qm10xd 硬件平台封装的 framebuffer 显示库，内部集成 MPP SYS/VO 初始化、FB 图层配置、压缩开启、mmap 等全流程。LVGL 通过 `XOSFB` 显示后端调用该库，替代标准 Linux fbdev 驱动。

## 架构

```
lv_port_linux
├── src/lib/display_backends/xosfb.c    ← LVGL XOSFB 后端 (FULL render mode)
├── src/lib/indev_backends/evdev.c      ← 输入后端 (event* fallback 机制)
├── xosfb/
│   ├── libxosfb.a                      ← 自包含静态库 (编译产出，需手动拷贝)
│   └── include/xosfb.h                 ← 公共 API 头文件
└── configs/xosfb.defaults              ← LVGL 配置 (LV_COLOR_DEPTH 32)
```

```
xosfb 库源码 (独立维护)
/mnt/qm/xos/base/soc/qm10xd/linux/media/sample/modules/xosfb/
├── xosfb.h          ← 公共 API
├── xosfb.c          ← 核心实现
└── Makefile         ← 产出 libxosfb.a
```

## 编译步骤

### 1. 编译 libxosfb.a (在构建服务器上)

```bash
cd /mnt/qm/xos/base/soc/qm10xd/linux/media/sample/modules/xosfb
make clean && make
```

### 2. 拷贝到 lv_port_linux

```bash
cp libxosfb.a /home/scm/github_gitee/lv_port_linux/xosfb/
```

### 3. 编译 lv_port_linux

```bash
cd /home/scm/github_gitee/lv_port_linux
./lvgl_build.sh
# 等效于: . qmenv qm10xd && cmake -B build -DCONFIG=xosfb && make -C build -j$(nproc)
```

## 运行时配置

### 像素格式

| 格式 | bpp | Alpha | 内存 (800×1280) | 适用场景 |
|------|-----|-------|-----------------|---------|
| `ARGB8888` (默认) | 32 | 8bit | ~4.1 MB | 推荐，无转换开销 |
| `ARGB0565` | 16 | 无 | ~2.0 MB | 低内存，不透明 UI |
| `ARGB1555` | 16 | 1bit | ~2.0 MB | 兼容旧格式 |

```bash
# 默认 ARGB8888
./lvglsim

# 切换格式
XOSFB_FORMAT=ARGB1555 ./lvglsim
XOSFB_FORMAT=ARGB0565 ./lvglsim
```

### 分辨率

```bash
# 默认 800×1280
# 自定义分辨率
LV_XOSFB_WIDTH=1024 LV_XOSFB_HEIGHT=600 ./lvglsim
```

### 输入设备

自动探测优先级：

1. `LV_LINUX_EVDEV_POINTER_DEVICE` 环境变量
2. `/dev/input/event0` → `/dev/input/event1` → `/dev/input/event2` 依次尝试
3. LVGL auto discovery 兜底

```bash
# 显式指定触摸屏
LV_LINUX_EVDEV_POINTER_DEVICE=/dev/input/event0 ./lvglsim

# 默认自动探测 (通常无需设置)
./lvglsim
```

### 回退到原始 fbdev 后端

```bash
./lvglsim -b FBDEV
```

## 关键设计决策 & 注意事项

### Render Mode: FULL

XOSFB 后端使用 `LV_DISPLAY_RENDER_MODE_FULL`（全帧渲染），而非 DIRECT（区域刷新）。

**原因：**
- qm10xd 硬件通过 `FBIOPAN_DISPLAY` 提交显示，这是一个全帧操作
- DIRECT 模式下逐区域 flush + 多次 PAN 会导致 buffer offset 漂移（画面偏移）
- FULL 模式在最后一帧完成后一次性 copy + PAN，与 `fb_test.c` 的 draw→pan 模式一致

**影响：**
- 每帧 LVGL 会重绘整个屏幕（而非仅脏区域），CPU 占用略高
- 显示正确性优先，性能影响在 qm10xd 上可接受

### 帧缓冲提交：FBIOPAN_DISPLAY

**必须调用** `FBIOPAN_DISPLAY`，否则显示全白。

- 该 ioctl 通知 VO 硬件刷新显示（BUF_NONE 模式下无硬件双缓冲）
- 只在 `lv_display_flush_is_last()` 为 true 时调用一次
- `yoffset` 始终设为 0

### 触摸屏：gslx680 兼容

`gslx680` 触摸控制器的 `KEY=0`（无按键支持），导致 LVGL 自动发现跳过该设备。evdev 后端增加了 fallback 机制：

```c
// 优先级:
// 1. LV_LINUX_EVDEV_POINTER_DEVICE 环境变量
// 2. /dev/input/event0, event1, event2 依次尝试
// 3. lv_evdev_discovery_start() 兜底
```

### 颜色格式映射

| xosfb 格式 | LVGL color_format | flush_cb 转换 |
|------------|-------------------|:---:|
| ARGB8888 | ARGB8888 | 直接 memcpy |
| ARGB0565 | RGB565 | 直接 memcpy |
| ARGB1555 | RGB565 | 逐像素 `\| 0x8000` 设置 alpha bit |

### libxosfb.a 自包含

库内已封装所有平台依赖：
- `libmpi.a` (MPP接口)
- `sample_comm_sys.o`, `sample_comm_vo.o` 等
- `fb_drv_ioc.h`, `fh_tde_mpi.h` 等私有头文件

lv_port_linux 只需 include `xosfb.h`，无需引入任何平台头文件。

### Tick 时钟

XOSFB 后端在 `init_xosfb()` 中调用 `lv_tick_set_cb()`，使用 `CLOCK_MONOTONIC` 作为时钟源。缺少 tick 会导致：
- 动画/Timer 不工作
- 输入事件不处理
- `lv_tick_inc() is not called` 警告

## 硬件设备清单

| 设备 | 路径 | 用途 |
|------|------|------|
| Framebuffer | `/dev/fb0` | 显示输出 (GRAPHICS_LAYER_G0) |
| VO 设备 | `/dev/vo` | Video Output 控制 |
| 触摸屏 | `/dev/input/event0` | gslx680 触摸输入 |
| 按键 | `/dev/input/event1` | gpio_keys 物理按键 |

## 调试

```bash
# 查看 LVGL 日志等级
# 编辑 configs/xosfb.defaults: LV_LOG_LEVEL LV_LOG_LEVEL_INFO

# 设备上测试触摸
cat /dev/input/event0 | hexdump -C   # 触摸屏幕看是否有事件输出
evtest /dev/input/event0              # 查看设备能力

# 查看进程打开的文件
lsof | grep lvglsim

# 查看 fb 信息
cat /proc/bus/input/devices

# 检查启动日志
./lvglsim 2>&1 | grep -E "xosfb|evdev|error|warn"
```

## 文件清单

```
lv_port_linux/
├── src/lib/display_backends/
│   ├── xosfb.c               # XOSFB 后端 (新建)
│   └── fbdev.c                # 原始 fbdev 后端 (保留)
├── src/lib/indev_backends/
│   └── evdev.c                # evdev 后端 (已修改: 增加 fallback)
├── src/lib/
│   ├── backends.h             # 已修改: 添加 backend_init_xosfb 声明
│   └── driver_backends.c      # 已修改: XOSFB 注册为首选后端
├── configs/
│   ├── xosfb.defaults         # XOSFB 配置 (新建)
│   └── fbdev.defaults         # 原始配置 (保留)
├── xosfb/
│   ├── libxosfb.a             # 自包含库 (由 Makefile 产出)
│   ├── include/xosfb.h        # API 头文件
│   └── README.md              # 编译说明
├── CMakeLists.txt             # 已修改: xosfb 自动检测和链接
└── lvgl_build.sh              # 已修改: CONFIG=xosfb
```

## 版本历史

| 日期 | 变更 |
|------|------|
| 2026-05-30 | 初始版本：xosfb 库封装、XOSFB 后端、多格式支持、evdev fallback、FULL render mode |
