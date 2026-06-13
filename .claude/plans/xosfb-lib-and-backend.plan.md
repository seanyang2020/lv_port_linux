# Plan: xosfb-lib.a 封装 + lv_port_linux xosfb 后端

**Source**: 用户需求
**Complexity**: Medium

## Summary

从 `common_fb/fb_test.c` 提取核心 framebuffer 操作逻辑，封装为自包含的 `libxosfb.a`
静态库（内部消化所有平台依赖：libmpi.a、sample_comm、私有 ioctl），对外只暴露简洁的
framebuffer 显示 API。然后在 lv_port_linux 中新建 `XOSFB` 后端，调用该库替代标准
fbdev 驱动。

## Patterns to Mirror

| Category | Source | Pattern |
|---|---|---|
| Backend 注册 | `src/lib/display_backends/fbdev.c:62-75` | `backend_init_*()` 函数签名 + `backend_t` 填充 |
| Backend 发现 | `src/lib/driver_backends.c:61-91` | `available_backends[]` 数组 + `#if` 条件编译 |
| flush_cb | `lvgl/src/drivers/display/fb/lv_linux_fbdev.c:308-431` | flush_cb 签名 + `lv_display_flush_is_last` + `lv_display_flush_ready` |
| API 命名 | `fb_test.c` | `xosfb_` 前缀，snake_case |
| 错误处理 | `fb_test.c` | 返回 `FY_S32`，0 成功，非 0 失败 |
| 日志 | `fb_test.c` | `printf` / `SAMPLE_PRT` / `perror` |

## Files to Change

| File | Action | Why |
|---|---|---|
| `/mnt/qm/xos/.../modules/xosfb/xosfb.h` | **CREATE** | 公共 API 头文件，lv_port_linux 唯一需要 include 的 |
| `/mnt/qm/xos/.../modules/xosfb/xosfb.c` | **CREATE** | 核心实现，封装 SYS/VO/FB 初始化全流程 |
| `/mnt/qm/xos/.../modules/xosfb/Makefile` | **CREATE** | 编译 libxosfb.a，静态链接 libmpi.a + sample_comm |
| `src/lib/display_backends/xosfb.c` | **CREATE** | LVGL 的 XOSFB 后端，调用 xosfb.h API |
| `src/lib/backends.h` | **UPDATE** | 添加 `backend_init_xosfb()` 声明 |
| `src/lib/driver_backends.c` | **UPDATE** | 注册 XOSFB 到 `available_backends[]` |
| `configs/xosfb.defaults` | **CREATE** | xosfb 配置片段 |
| `CMakeLists.txt` | **UPDATE** | 添加 xosfb-lib.a 链接和 include 路径 |
| `lvgl_build.sh` | **UPDATE** | 使用 `CONFIG=xosfb` 参数 |

## xosfb.h 公共 API 设计

```c
#ifndef XOSFB_H
#define XOSFB_H

/** 像素格式枚举 */
typedef enum {
    XOSFB_FMT_ARGB8888 = 0,   /**< A:R:G:B = 8:8:8:8, 32bpp, 默认 */
    XOSFB_FMT_ARGB1555,       /**< A:R:G:B = 1:5:5:5, 16bpp */
    XOSFB_FMT_ARGB0565,       /**< A:R:G:B = 0:5:6:5, 16bpp (即 RGB565) */
    XOSFB_FMT_COUNT
} xosfb_pixel_format_t;

typedef struct xosfb_ctx xosfb_ctx_t;

/** 初始化整个显示系统：SYS + VO + FB open + layer setup + mmap
 *  @param width  屏幕宽度
 *  @param height 屏幕高度
 *  @param fmt    像素格式，默认 XOSFB_FMT_ARGB8888
 *  @return 上下文句柄，失败返回 NULL
 */
xosfb_ctx_t *xosfb_init(int width, int height, xosfb_pixel_format_t fmt);

/** 销毁，逆序释放所有资源 */
void xosfb_exit(xosfb_ctx_t *ctx);

/** 获取 mmap 的 framebuffer 指针，LVGL 直接写入此内存 */
void *xosfb_get_fb_ptr(xosfb_ctx_t *ctx);

/** 获取 framebuffer 一行字节数 (stride) */
int xosfb_get_line_length(xosfb_ctx_t *ctx);

/** 获取分辨率 */
void xosfb_get_resolution(xosfb_ctx_t *ctx, int *w, int *h);

/** 获取像素位深 */
int xosfb_get_bpp(xosfb_ctx_t *ctx);

/** 获取像素格式 */
xosfb_pixel_format_t xosfb_get_pixel_format(xosfb_ctx_t *ctx);

/** 提交显示：将 framebuffer 内容刷新到屏幕 (底层调用 FBIOPAN_DISPLAY) */
void xosfb_pan_display(xosfb_ctx_t *ctx);

/** 显示/隐藏图层 */
void xosfb_show(xosfb_ctx_t *ctx, int enable);

#endif /* XOSFB_H */
```

### 内部 bitfield 配置表

xosfb.c 内部根据格式枚举自动设置 `fb_var_screeninfo` 的色域：

| 格式 | bpp | transp (A) | red (R) | green (G) | blue (B) |
|------|-----|------------|---------|-----------|----------|
| `XOSFB_FMT_ARGB8888` | 32 | {24,8,0} | {16,8,0} | {8,8,0} | {0,8,0} |
| `XOSFB_FMT_ARGB1555` | 16 | {15,1,0} | {10,5,0} | {5,5,0} | {0,5,0} |
| `XOSFB_FMT_ARGB0565` | 16 | {0,0,0}  | {11,5,0} | {5,6,0} | {0,5,0} |

### LVGL 侧格式映射

XOSFB 后端根据 `xosfb_get_pixel_format()` 返回值设置 LVGL 颜色格式：

| xosfb 格式 | LVGL `color_format` | LVGL `LV_COLOR_DEPTH` |
|------------|---------------------|-----------------------|
| `ARGB8888` (默认) | `LV_COLOR_FORMAT_ARGB8888` | 32 |
| `ARGB1555` | `LV_COLOR_FORMAT_RGB565` + flush 中软件转换 | 16 |
| `ARGB0565` | `LV_COLOR_FORMAT_RGB565` | 16 |

## Tasks

### Task 1: 创建 xosfb.c/h 和 Makefile，编译 libxosfb.a

- **Action**:
  - 在 `/mnt/qm/xos/base/soc/qm10xd/linux/media/sample/modules/xosfb/` 创建源文件
  - 从 `fb_test.c` 提取核心路径，按 `fmt` 参数选择 bitfield 配置：
    1. `FB_COMM_SYS_Init` → `FB_COMM_VO_Init`
    2. `open(/dev/fb0)` → `FBIOPUT_SHOW_FYFB(false)`
    3. 根据 `fmt` 设置 `var.transp/red/green/blue/bits_per_pixel`（见上方配置表）
    4. `FBIOPUT_VSCREENINFO` → `FBIOGET_FSCREENINFO`
    5. `FBIOPUT_LAYER_INFO` → `FBIOPUT_COMPRESSION_FYFB`
    6. `mmap` → `FBIOPUT_SHOW_FYFB(true)`
  - `xosfb_init(width, height, fmt)` — 默认 `XOSFB_FMT_ARGB8888`
  - Makefile 静态链接 `libmpi.a` 和 `sample_comm_*.o`，产出 `libxosfb.a`
- **Mirror**: `common_fb/Makefile` 的 CFLAGS 和链接参数
- **Validate**: `file libxosfb.a` 确认为 ARM 静态库

### Task 2: 创建 lv_port_linux 的 XOSFB 后端

- **Action**:
  - 创建 `src/lib/display_backends/xosfb.c`，实现 `backend_init_xosfb()`
  - `init_xosfb()`:
    1. 通过环境变量 `LV_XOSFB_FORMAT` 读取格式选择（默认 `ARGB8888`）
    2. 调用 `xosfb_init(w, h, fmt)` → 获取分辨率/fb指针/格式
    3. 根据 `xosfb_get_pixel_format()` 设置 LVGL `color_format`：
       - `ARGB8888` → `LV_COLOR_FORMAT_ARGB8888`
       - `ARGB1555` → `LV_COLOR_FORMAT_RGB565`（需 flush 转换）
       - `ARGB0565` → `LV_COLOR_FORMAT_RGB565`
    4. `lv_display_create()` → 设置 flush_cb
  - `run_loop_xosfb()`: 标准 `lv_timer_handler()` 循环
  - `flush_cb`: 写入数据到 `xosfb_get_fb_ptr()` 返回的地址，仅 `is_last_flush` 时调用 `xosfb_pan_display()`
  - ARGB1555 时 flush 做逐像素转换：RGB565 → ARGB1555（设置 bit15=1）
- **Mirror**: 后端注册模式来自 `fbdev.c`，flush 模式来自 LVGL 标准驱动
- **Validate**: 编译通过

### Task 3: 注册后端 + CMakeLists 适配

- **Action**:
  - `backends.h` 添加 `backend_init_xosfb()` 声明
  - `driver_backends.c` 在 `available_backends[]` 中添加 `#if LV_USE_LINUX_FBDEV` → `backend_init_xosfb` 条目（复用现有宏或新增 `LV_USE_XOSFB`）
  - `CMakeLists.txt` 添加 `libxosfb.a` 链接、SYSROOT include 路径
  - 创建 `configs/xosfb.defaults` 配置文件
- **Validate**: `./lvgl_build.sh` 编译成功

### Task 4: 验证

- **Action**: 编译产物拷贝到设备运行，确认 LVGL demo 正常显示
- **Validate**: 设备上 LVGL widget demo 正常渲染

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| flush_cb 逐区域刷新 vs FBIOPAN_DISPLAY 全屏翻转不匹配 | 中 | 只在 `lv_display_flush_is_last()` 为 true 时调用 PAN |
| ARGB1555 时 flush 需逐像素转换，有性能开销 | 低 | 默认用 ARGB8888（零开销），ARGB1555 仅兼容旧场景 |
| ARGB0565 丢 alpha 导致透明效果异常 | 低 | 文档明确标注此格式无 alpha，供不需要透明的场景使用 |
| libmpi.a 链接符号冲突 | 低 | 静态库封装隔离 |
| xosfb 需要与 evdev 配合才能响应触摸 | 低 | 保持现有 evdev 后端不变，仅替换 display 后端 |

## Acceptance

- [ ] `libxosfb.a` 成功编译，包含所有平台依赖
- [ ] lv_port_linux 新增 `XOSFB` 后端，通过 `-DCONFIG=xosfb` 构建
- [ ] LVGL demo 在设备上正常显示
- [ ] 原有 fbdev 后端保持不变，可同时存在
