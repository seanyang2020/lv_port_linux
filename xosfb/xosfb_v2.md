# xosfb v2 — 硬件加速帧缓冲库 (AI 集成手册)

> **面向**: Claude / AI Coding Agent  
> **版本**: v2.0 (独立版) | **平台**: qm10xd (FH8626) | **日期**: 2026-06-15  
> **依赖**: `libmpi.a` (TDE2 + VGS2 + VB + SYS), `linux-fb`  
> **v1 关系**: **完全独立，不需要 xosfb.h / xosfb.c / libxosfb.a**

---

## 0. Claude Quick-Start (30 秒速览)

**xosfb_v2 是什么**: 独立的帧缓冲 + 硬件加速库，替代 v1。TDE2/VGS2 硬件加速 fill/copy/blit/rotate。

**集成只需 2 个文件**:
```
xosfb_v2.h          ← 单头文件 (#include 这一个即可)
libxosfb_v2.a       ← 单静态库 (自包含，含 libmpi.a 对象)
```

**最小代码**:
```c
#include "xosfb_v2.h"

xosfb_ctx_t *ctx = xosfb_v2_init(800, 480, XOSFB_FMT_ARGB1555);
xosfb_v2_fill_rect(ctx, 0, 0, 800, 480, 0xFC00);  // 红色清屏
xosfb_pan_display(ctx);
xosfb_v2_exit(ctx);
```

**⚠️ 最重要约束**: `xosfb_v2_blit()` / `xosfb_v2_rotate_blit()` 的 `src_buf` **必须**是 DMA 内存 (`xosfb_v2_alloc_dma`)。malloc 内存 → 黑屏。

---

## 1. API 决策树

```
需要往 FB 写数据?
│
├─ 填充纯色矩形?
│   └─ xosfb_v2_fill_rect(ctx, x, y, w, h, color)
│      TDE2 硬件 ~0.3ms/全屏
│      ⚠️ color 是 FB 原生格式, 检查 xosfb_get_bpp()
│
├─ 拷贝 FB 内区域?
│   └─ xosfb_v2_copy_rect(ctx, sx, sy, w, h, dx, dy)
│      TDE2 硬件 ~1ms/全屏
│
├─ 外部 buffer → FB (格式/尺寸可能不同)?
│   └─ xosfb_v2_blit(ctx, &desc)
│      VGS2 硬件 ~2ms/全屏
│      ⚠️ src_buf 必须 DMA 内存!
│
├─ 旋转画面?
│   └─ xosfb_v2_rotate_blit(ctx, src, w, h, fmt, dx, dy, rot)
│      VGS2 硬件 ~2.5ms/全屏
│      ⚠️ src_buf 必须 DMA 内存!
│
└─ 需要 DMA buffer?
    ├─ xosfb_v2_alloc_dma(ctx, size, &buf)
    └─ xosfb_v2_free_dma(ctx, &buf)
```

---

## 2. 集成清单 (逐条打勾)

### 2.1 文件准备

```
[ ] 复制到工程:
    [ ] xosfb_v2.h                      ← include 目录
    [ ] libxosfb_v2.a                   ← lib 目录

注意: 不需要 xosfb.h / xosfb.c / libxosfb.a (v1 文件)
```

### 2.2 编译配置

```
[ ] 头文件路径 (编译 xosfb_v2.c 或确保 libxosfb_v2.a 可链接):
    [ ] -I$(MPP_DIR)/include             (fh_*.h)
    [ ] -I$(MPP_DIR)/include/dsp         (fh_tde_mpi.h 等)
    [ ] -I$(MPP_DIR)/drv_include         (fb_drv_ioc.h)

[ ] 链接:
    [ ] libxosfb_v2.a                     (自包含, 含 libmpi.a 对象)
    [ ] -lpthread -lm -ldl                (系统库)

注意: 不需要单独链接 -lmpi (已合并在 .a 内)
```

### 2.3 代码替换

```
[ ] 头文件:
    [ ] 删除 #include "xosfb.h"
    [ ] 添加 #include "xosfb_v2.h"

[ ] 初始化:
    [ ] xosfb_init(w,h,fmt) → xosfb_v2_init(w,h,fmt)
    [ ] xosfb_exit(ctx)     → xosfb_v2_exit(ctx)

[ ] CPU 操作 → 硬件加速:
    [ ] memset / for-loop fill → xosfb_v2_fill_rect()
    [ ] 逐行 memcpy            → xosfb_v2_blit() (需 DMA buffer)
```

### 2.4 验证

```
[ ] xosfb_v2_init() 返回非 NULL (即使 MPP 已预初始化也正常)
[ ] xosfb_v2_get_caps() 返回预期能力位
[ ] 画面正常, 无花屏/黑屏
```

---

## 3. ⚠️ 关键约束与常见陷阱

### 陷阱 1: DMA 内存 vs 普通内存 (最高频错误)

```c
// ❌ malloc 内存 → VGS2 无法访问 → 黑屏
uint8_t *buf = malloc(W * H * 4);
desc.src_buf = buf;  // 错误!

// ✅ DMA 内存
xosfb_v2_dma_buf_t dma;
xosfb_v2_alloc_dma(ctx, W * H * 4, &dma);
desc.src_buf = dma.virt_addr;  // 正确
```

### 陷阱 2: fill_rect 的颜色格式

```c
// ❌ 假设总是 ARGB8888
xosfb_v2_fill_rect(ctx, 0,0,100,100, 0xFFFF0000);

// ✅ 匹配 FB 格式
if (xosfb_get_bpp(ctx) == 32)
    xosfb_v2_fill_rect(ctx, 0,0,100,100, 0xFFFF0000); // ARGB8888
else
    xosfb_v2_fill_rect(ctx, 0,0,100,100, 0xFC00);     // ARGB1555
```

### 陷阱 3: 忘记 pan_display

```c
xosfb_v2_fill_rect(ctx, 0,0,800,480, 0xFF000000);
// 画面不更新! 数据在内存但未提交

xosfb_v2_fill_rect(ctx, 0,0,800,480, 0xFF000000);
xosfb_pan_display(ctx);  // ← 必须有
```

### 陷阱 4: 如果 MPP 已预初始化

v2 会自动容错——`FH_VB_SetConf` 或 `FH_VO_SetPubAttr` 失败时仅打印警告并继续。**不再需要手动跳过 MPP/VO 初始化。**

但如果运行在完全无 MPP 的环境中:
```
[ ] 确认 libmpi.a 对应的内核驱动已加载
[ ] dmesg | grep -i "tde\|vgs\|vb\|vo"
```

---

## 4. 错误码速查

| 返回 | 含义 | 排查 |
|:-----|------|------|
| 0 | 成功 | — |
| `-EINVAL` | 参数无效 | ctx=NULL, w/h≤0 |
| `-ENODEV` | 硬件不可用 | 检查 `caps`, 确认 TDE/VGS 驱动已加载 |
| `-ENOMEM` | 内存不足 | VB 池耗尽, 及时释放 DMA buf |
| `-EIO` | 硬件操作失败 | dmesg 查看 TDE/VGS 错误 |

---

## 5. API 速查表

### 初始化

| 函数 | 必须? | 说明 |
|------|:-----:|------|
| `xosfb_v2_init(w, h, fmt)` | ✅ | 初始化一切 (容错已预初始化的 MPP) |
| `xosfb_v2_exit(ctx)` | ✅ | 释放一切 (只释放自己初始化的) |
| `xosfb_v2_get_caps(ctx)` | 可选 | 查询硬件能力 |

### 渲染

| 函数 | 硬件 | 约束 |
|------|:---:|------|
| `xosfb_v2_fill_rect(ctx, x, y, w, h, color)` | TDE2 | color=FB 原生格式 |
| `xosfb_v2_copy_rect(ctx, sx, sy, w, h, dx, dy)` | TDE2 | 源/目标均在 FB 内 |
| `xosfb_v2_blit(ctx, &desc)` | VGS2 | **src_buf=DMA 内存** |
| `xosfb_v2_rotate_blit(ctx, ...)` | VGS2 | **src=DMA 内存** |

### 内存

| 函数 | 说明 |
|------|------|
| `xosfb_v2_alloc_dma(ctx, size, &buf)` | 分配 MMZ 物理连续内存 |
| `xosfb_v2_free_dma(ctx, &buf)` | 释放 |

### FB 访问

| 函数 | 说明 |
|------|------|
| `xosfb_get_fb_ptr(ctx)` | FB mmap 虚拟地址 |
| `xosfb_get_resolution(ctx, &w, &h)` | 分辨率 |
| `xosfb_get_bpp(ctx)` | bpp (16/32) |
| `xosfb_get_pixel_format(ctx)` | 格式枚举 |
| `xosfb_get_line_length(ctx)` | 行字节跨度 |
| `xosfb_pan_display(ctx)` | 提交到显示 |
| `xosfb_show(ctx, enable)` | 显示/隐藏图层 |

---

## 6. LVGL 完整集成模板

```c
#include "xosfb_v2.h"
#include "lvgl/lvgl.h"

static xosfb_ctx_t *g_ctx = NULL;
static xosfb_v2_dma_buf_t g_lvgl_dma = {0};

#define LCD_W  800
#define LCD_H  480
/* FB 用 16bpp 省带宽; LVGL 用 32bpp 最佳画质 — VGS2 硬件转换 */
#define FB_FMT   XOSFB_FMT_ARGB1555
#define LVGL_FMT XOSFB_FMT_ARGB8888

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    int w = lv_area_get_width(area);
    int h = lv_area_get_height(area);
    if (w <= 0 || h <= 0) { lv_display_flush_ready(disp); return; }

    xosfb_v2_blit_desc_t d = {
        .src_buf = px, .src_w = w, .src_h = h,
        .src_stride = LCD_W, .src_fmt = LVGL_FMT,
        .dst_x = area->x1, .dst_y = area->y1,
        .dst_w = w, .dst_h = h,
    };
    xosfb_v2_blit(g_ctx, &d);

    if (lv_display_flush_is_last(disp)) xosfb_pan_display(g_ctx);
    lv_display_flush_ready(disp);
}

int setup_lvgl(void)
{
    g_ctx = xosfb_v2_init(LCD_W, LCD_H, FB_FMT);
    if (!g_ctx) return -1;

    /* 格式不同 → 必须用 DMA buffer */
    xosfb_v2_alloc_dma(g_ctx, LCD_W * LCD_H * 4, &g_lvgl_dma);

    lv_init();
    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_buffers(disp, g_lvgl_dma.virt_addr, NULL,
                           g_lvgl_dma.size, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    return 0;
}

void teardown_lvgl(void)
{
    xosfb_v2_free_dma(g_ctx, &g_lvgl_dma);
    xosfb_v2_exit(g_ctx);
}
```

---

## 7. 故障排查表

| 症状 | 原因 | 检查 |
|------|------|------|
| `xosfb_v2_init()` → NULL | /dev/fb0 不存在 | `ls -la /dev/fb0` |
| `FH_VB_SetConf failed` 日志 | MPP 已预初始化 | **正常!** v2 会继续运行 |
| blit 后黑屏 | src_buf 不是 DMA 内存 | 确认用了 xosfb_v2_alloc_dma() |
| fill_rect 颜色异常 | color 与 FB 格式不匹配 | 检查 xosfb_get_bpp() |
| blit → -ENODEV | VGS2 未就绪 | 检查 caps |
| alloc_dma → -ENOMEM | VB 池耗尽 | 及时释放或增大 u32BlkCnt |
| 编译缺 fh_*.h | MPI 头路径未配 | -I$(MPP_DIR)/include/dsp |
| 链接缺 FH_TDE2_Open | libmpi.a 未在库中 | 确认用的是新版 libxosfb_v2.a |
| 链接缺 SAMPLE_COMM_* | 用了旧版 .a | 替换为新版 libxosfb_v2.a |

---

## 8. 文件清单

```
xosfb v2 独立交付物 (仅 2 个文件):
├── xosfb_v2.h          ← 单头文件, #include 这一个即可
└── libxosfb_v2.a       ← 单库文件, 自包含 (xosfb_v2.o + libmpi.a)

v1 遗留文件 (v2 不需要):
├── xosfb.h / xosfb.c / libxosfb.a   ← v1 版本, v2 不依赖
├── Makefile / Makefile_v2           ← 编译脚本
└── xosfb_v2.md                      ← 本文档
```

**集成到第三方工程**只需复制 `xosfb_v2.h` + `libxosfb_v2.a` 两个文件。

---

## 9. 参考链接

| 资源 | 路径 |
|------|------|
| v2 头文件 | `xosfb_v2.h` |
| v2 实现 | `xosfb_v2.c` |
| TDE2 MPI 头 | `$(MPP_DIR)/include/dsp/fh_tde_mpi.h` |
| VGS2 MPI 头 | `$(MPP_DIR)/include/dsp/fh_vgs2_mpi.h` |
| VB MPI 头 | `$(MPP_DIR)/include/dsp/fh_vb_mpi.h` |
| FB ioctl 定义 | `$(MPP_DIR)/drv_include/fb_drv_ioc.h` |
