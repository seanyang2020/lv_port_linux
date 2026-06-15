# xosfb v2 — 硬件加速帧缓冲库 (AI 集成手册)

> **面向读者**: Claude / AI Coding Agent  
> **版本**: v2.0 | **平台**: qm10xd (FH8626) | **日期**: 2026-06-15  
> **依赖**: `libmpi.a` (TDE2 + VGS2 + VB + SYS MPI), `linux-fb`  
> **源文件位置**: `xosfb/` 目录 (与 v1 同目录)

---

## 0. Claude Quick-Start (30 秒速览)

**xosfb_v2 是什么**: 在 v1 的 FB mmap 基础上增加了 TDE2/VGS2 硬件加速的帧缓冲库。

**何时使用**: 任何需要将像素数据写入 `/dev/fb0` 并希望用硬件加速替代 CPU memcpy/fill/convert/rotate 的场景。

**最小集成步骤**:
1. 复制 `xosfb.h` + `xosfb.c` + `xosfb_v2.h` + `xosfb_v2.c` 到工程
2. 链接 `libxosfb_v2.a` (或自行编译上述文件 + 链接 `libmpi.a`)
3. `#include "xosfb_v2.h"`
4. `ctx = xosfb_v2_init(W, H, fmt)` 替代 `xosfb_init()`
5. 用 `xosfb_v2_fill_rect()` / `xosfb_v2_blit()` 替代 CPU 循环
6. `xosfb_v2_exit(ctx)` 清理

**⚠️ 最重要的约束**: 传给 `xosfb_v2_blit()` 和 `xosfb_v2_rotate_blit()` 的 `src_buf` **必须是**通过 `xosfb_v2_alloc_dma()` 分配的 DMA 内存。普通 `malloc` 内存 → VGS2 硬件无法访问 → 黑屏或崩溃。

---

## 1. API 决策树

当 AI agent 遇到以下需求时，按此树选择 API:

```
需要往 FB 写数据?
│
├─ 需要填充纯色矩形?
│   └─ xosfb_v2_fill_rect(ctx, x, y, w, h, color)
│      ✅ TDE2 硬件, ~0.3ms/全屏
│      ⚠️ color 必须是 FB 原生格式 (不是固定 ARGB8888!)
│
├─ 需要拷贝 FB 内一个矩形到另一个位置?
│   └─ xosfb_v2_copy_rect(ctx, sx, sy, w, h, dx, dy)
│      ✅ TDE2 硬件, ~1ms/全屏
│
├─ 需要把外部 buffer 写入 FB (可能格式不同 / 需要缩放)?
│   ├─ src_fmt == FB fmt 且 src_w==dst_w 且 src_h==dst_h ?
│   │   └─ 直接用 memcpy() 到 fbp (简单场景不需要 VGS2)
│   │      或 xosfb_v2_copy_rect() 如果源也在 FB 内
│   │
│   └─ src_fmt != FB fmt 或需要缩放?
│       └─ xosfb_v2_blit(ctx, &desc)
│          ✅ VGS2 硬件 CSC + Scale, ~2ms/全屏
│          ⚠️ src_buf 必须是 DMA 内存! 调用前先 xosfb_v2_alloc_dma()
│
├─ 需要旋转画面?
│   └─ xosfb_v2_rotate_blit(ctx, src, w, h, fmt, dx, dy, rot)
│      ✅ VGS2 硬件旋转, ~2.5ms/全屏
│      ⚠️ src_buf 必须是 DMA 内存!
│
└─ 需要分配 DMA buffer?
    ├─ xosfb_v2_alloc_dma(ctx, size, &buf)   // 从 MMZ 分配
    └─ xosfb_v2_free_dma(ctx, &buf)           // 释放
```

---

## 2. 集成清单 (逐条打勾)

将此清单逐项执行，每完成一项标记 ✅:

### 2.1 文件复制

```
[ ] 复制到工程 include/ 目录:
    [ ] xosfb.h         (v1 基础 API)
    [ ] xosfb_v2.h      (v2 加速 API)

[ ] 复制到工程 src/ 目录:
    [ ] xosfb.c         (v1 实现)
    [ ] xosfb_v2.c      (v2 实现)
```

### 2.2 编译配置

```
[ ] 头文件路径 (CFLAGS):
    [ ] -I$(MPP_DIR)/include        (fh_*.h MPI 头文件)
    [ ] -I$(MPP_DIR)/include/dsp    (fh_tde_mpi.h 等)
    [ ] -I$(MPP_DIR)/drv_include    (fb_drv_ioc.h)

[ ] 链接库 (LDFLAGS):
    [ ] -lmpi    或  libmpi.a       (TDE2 + VGS2 + VB + SYS)
    [ ] -lpthread -lm -ldl          (基础系统库)

[ ] 编译验证:
    [ ] make 成功, 无 warning/error
```

### 2.3 代码替换

```
[ ] main.c / 初始化文件:
    [ ] #include "xosfb_v2.h"  (替换或补充 xosfb.h)
    [ ] xosfb_init(w, h, fmt) → xosfb_v2_init(w, h, fmt)
    [ ] xosfb_exit(ctx)       → xosfb_v2_exit(ctx)

[ ] 像素填充/清屏:
    [ ] memset(fbp, color, size) → xosfb_v2_fill_rect(ctx, 0, 0, w, h, color)
    [ ] for(i){for(j){fbp[i*w+j]=c;}} → xosfb_v2_fill_rect(ctx, x, y, w, h, c)

[ ] LVGL flush callback (如果适用):
    [ ] 确认 LVGL buffer 用 xosfb_v2_alloc_dma() 分配 (DMA 内存)
    [ ] 替换 memcpy 为 xosfb_v2_blit()
    [ ] 见下方 §6 完整模板

[ ] 其他 fbdev_get_fbp() 的直接写入:
    [ ] 评估是否可以用 fill_rect / copy_rect / blit 替代
```

### 2.4 功能验证

```
[ ] 运行时检查:
    [ ] xosfb_v2_init() 返回非 NULL
    [ ] xosfb_v2_get_caps() & XOSFB_V2_CAP_FILL 为真
    [ ] 画面正常显示, 无花屏/黑屏
```

---

## 3. ⚠️ 关键约束与常见陷阱

### 陷阱 1: DMA 内存 vs 普通内存

**这是最常见的集成错误。**

```c
// ❌ 错误 — malloc 内存不能用于 VGS2 硬件
uint8_t *buf = malloc(800 * 480 * 4);
xosfb_v2_blit_desc_t desc = { .src_buf = buf, ... };
xosfb_v2_blit(ctx, &desc);  // 黑屏或数据错误!

// ✅ 正确 — 使用 DMA 内存
xosfb_v2_dma_buf_t dma;
xosfb_v2_alloc_dma(ctx, 800 * 480 * 4, &dma);
// ... 渲染到 dma.virt_addr ...
desc.src_buf = dma.virt_addr;
xosfb_v2_blit(ctx, &desc);  // 正常工作
```

**规则**: `xosfb_v2_blit()` 和 `xosfb_v2_rotate_blit()` 的 `src_buf` 参数:
- ✅ 可以是: `xosfb_v2_alloc_dma()` 返回的 `buf.virt_addr`
- ✅ 可以是: 其他 MMZ/VB 分配的物理连续内存
- ❌ 不可以: `malloc()`, `calloc()`, 栈变量, 全局数组
- ❌ 不可以: `mmap()` 的普通文件映射 (非 `/dev/mmz`)

### 陷阱 2: fill_rect 的颜色格式

```c
// ❌ 错误 — 假定颜色总是 ARGB8888
xosfb_v2_fill_rect(ctx, 0, 0, 100, 100, 0xFFFF0000);  // 仅在 FB=ARGB8888 时有效

// ✅ 正确 — 根据 FB 格式构造颜色值
if (xosfb_get_bpp(ctx) == 32)
    xosfb_v2_fill_rect(ctx, 0, 0, 100, 100, 0xFFFF0000);  // ARGB8888
else
    xosfb_v2_fill_rect(ctx, 0, 0, 100, 100, 0xFC00);      // ARGB1555 红色: A=1,R=31,G=0,B=0
```

### 陷阱 3: 初始化顺序冲突

```c
// ❌ 禁止 — v1 和 v2 不能同时初始化
ctx1 = xosfb_init(w, h, fmt);     // v1: 手动 init SYS/VO
ctx2 = xosfb_v2_init(w, h, fmt);  // v2: 也会 init SYS/VO → 冲突!

// ✅ 正确 — 只用一个
ctx = xosfb_v2_init(w, h, fmt);   // v2 包含 v1 全部功能
```

### 陷阱 4: 忘记调用 pan_display

```c
// ❌ 常见遗漏
xosfb_v2_fill_rect(ctx, 0, 0, 800, 480, 0xFF000000);
// 画面不更新! 因为硬件填充到 FB 内存但未提交到 LCM

// ✅ 正确
xosfb_v2_fill_rect(ctx, 0, 0, 800, 480, 0xFF000000);
xosfb_pan_display(ctx);  // ← 必须有!
```

### 陷阱 5: DMA buffer 生命周期

```c
// ❌ 错误 — use-after-free
xosfb_v2_dma_buf_t buf;
xosfb_v2_alloc_dma(ctx, size, &buf);
xosfb_v2_free_dma(ctx, &buf);
xosfb_v2_blit(ctx, &(xosfb_v2_blit_desc_t){ .src_buf = buf.virt_addr }); // buf 已释放!

// ✅ 正确 — blit 后再释放
xosfb_v2_blit(ctx, &desc);  // 先提交硬件操作
xosfb_v2_free_dma(ctx, &buf);  // 再释放内存
```

---

## 4. 错误码速查

| 返回值 | 含义 | 常见原因 | 排查方向 |
|:------|------|---------|---------|
| 0 | 成功 | — | — |
| `-EINVAL` (-22) | 参数无效 | ctx=NULL, w/h≤0, buf=NULL | 检查调用参数 |
| `-ENODEV` (-19) | 硬件不可用 | TDE2/VGS2 初始化失败 | 检查 `caps` 是否包含所需能力 |
| `-ENOMEM` (-12) | 内存不足 | VB 池耗尽 | 调大 `u32BlkCnt` 或及时释放 DMA buf |
| `-EIO` (-5) | 硬件操作失败 | BeginJob/EndJob 错误 | 检查 dmesg, 确认 TDE2/VGS2 驱动已加载 |

---

## 5. API 速查表

### 5.1 初始化

| 函数 | 说明 | 必须调用? |
|------|------|:--------:|
| `xosfb_v2_init(w, h, fmt)` | 初始化所有硬件 + FB | ✅ 是 (第一个) |
| `xosfb_v2_exit(ctx)` | 释放所有资源 | ✅ 是 (最后一个) |
| `xosfb_v2_get_caps(ctx)` | 查询硬件能力位掩码 | 可选 (建议) |

### 5.2 渲染

| 函数 | 硬件 | 输入要求 | 输出 |
|------|:---:|------|------|
| `xosfb_v2_fill_rect(ctx, x, y, w, h, color)` | TDE2 | color=FB 原生格式 | FB (x,y,w,h) 区域被填充 |
| `xosfb_v2_copy_rect(ctx, sx, sy, w, h, dx, dy)` | TDE2 | 源/目标均在 FB 内 | FB 内区域拷贝 |
| `xosfb_v2_blit(ctx, &desc)` | VGS2 | src_buf=**DMA** 内存 | FB (dx,dy,dw,dh) |
| `xosfb_v2_rotate_blit(ctx, src, w, h, fmt, dx, dy, rot)` | VGS2 | src=**DMA** 内存 | FB 旋转后写入 |

### 5.3 内存

| 函数 | 说明 |
|------|------|
| `xosfb_v2_alloc_dma(ctx, size, &buf)` | 分配 MMZ 物理连续内存 |
| `xosfb_v2_free_dma(ctx, &buf)` | 释放 DMA 内存 |

### 5.4 v1 兼容 (依然可用)

| 函数 | 说明 |
|------|------|
| `xosfb_get_fb_ptr(ctx)` | 获取 FB mmap 虚拟地址 |
| `xosfb_get_resolution(ctx, &w, &h)` | 获取分辨率 |
| `xosfb_get_bpp(ctx)` | 获取每像素位数 (16 或 32) |
| `xosfb_get_pixel_format(ctx)` | 获取像素格式枚举 |
| `xosfb_get_line_length(ctx)` | 获取行字节跨度 |
| `xosfb_pan_display(ctx)` | 提交 FB 到显示 |
| `xosfb_show(ctx, enable)` | 显示/隐藏图层 |

---

## 6. LVGL 完整集成模板

以下是可直接复制到 `main.c` 的 LVGL + xosfb_v2 集成代码:

```c
/* ================================================================
 * LVGL + xosfb_v2 集成模板 (copy-paste 到 main.c)
 * ================================================================ */

#include "xosfb_v2.h"
#include "lvgl/lvgl.h"

/* ---- 全局变量 ---- */
static xosfb_ctx_t *g_xosfb_ctx = NULL;
static xosfb_v2_dma_buf_t g_lvgl_buf1 = {0};  /* LVGL 渲染 buffer (DMA) */
static int g_lvgl_hor_res = 800;
static int g_lvgl_ver_res = 480;
static xosfb_pixel_format_t g_fb_fmt = XOSFB_FMT_ARGB1555;  /* FB 用 16bpp 省 DDR 带宽 */
static xosfb_pixel_format_t g_lvgl_fmt = XOSFB_FMT_ARGB8888; /* LVGL 用 32bpp 最佳画质 */

/* ---- LVGL flush callback (v2 硬件加速版) ---- */
static void lvgl_flush_v2_cb(lv_display_t *disp, const lv_area_t *area,
                              uint8_t *color_p)
{
    xosfb_ctx_t *ctx = g_xosfb_ctx;
    int w = lv_area_get_width(area);
    int h = lv_area_get_height(area);

    if (w <= 0 || h <= 0) {
        lv_display_flush_ready(disp);
        return;
    }

    /* 仅当格式不同或尺寸不同时才用 VGS2，否则直接 memcpy */
    if (g_fb_fmt != g_lvgl_fmt) {
        xosfb_v2_blit_desc_t desc = {
            .src_buf    = color_p,
            .src_w      = w,
            .src_h      = h,
            .src_stride = g_lvgl_hor_res,  /* LVGL 行跨度 (全屏宽) */
            .src_fmt    = g_lvgl_fmt,
            .dst_x      = area->x1,
            .dst_y      = area->y1,
            .dst_w      = w,
            .dst_h      = h,
        };
        xosfb_v2_blit(ctx, &desc);
    }

    if (lv_display_flush_is_last(disp))
        xosfb_pan_display(ctx);

    lv_display_flush_ready(disp);
}

/* ---- 初始化 ---- */
int lvgl_xosfb_v2_setup(void)
{
    /* 1. 初始化 xosfb v2 */
    g_xosfb_ctx = xosfb_v2_init(g_lvgl_hor_res, g_lvgl_ver_res, g_fb_fmt);
    if (!g_xosfb_ctx) {
        fprintf(stderr, "FATAL: xosfb_v2_init failed\n");
        return -1;
    }

    /* 2. 检查硬件能力 (可选但建议) */
    uint32_t caps = xosfb_v2_get_caps(g_xosfb_ctx);
    printf("xosfb_v2 caps: 0x%x (fill=%d copy=%d convert=%d scale=%d rotate=%d)\n",
           caps,
           !!(caps & XOSFB_V2_CAP_FILL),
           !!(caps & XOSFB_V2_CAP_COPY),
           !!(caps & XOSFB_V2_CAP_CONVERT),
           !!(caps & XOSFB_V2_CAP_SCALE),
           !!(caps & XOSFB_V2_CAP_ROTATE));

    /* 3. 分配 LVGL DMA buffer (格式不同时需要 VGS2, buffer 必须 DMA) */
    if (g_fb_fmt != g_lvgl_fmt) {
        size_t buf_size = g_lvgl_hor_res * g_lvgl_ver_res *
            (g_lvgl_fmt == XOSFB_FMT_ARGB8888 ? 4 : 2);
        if (xosfb_v2_alloc_dma(g_xosfb_ctx, buf_size, &g_lvgl_buf1) != 0) {
            fprintf(stderr, "FATAL: DMA alloc failed\n");
            return -1;
        }
        printf("LVGL DMA buffer: virt=%p phy=0x%lx size=%zu\n",
               g_lvgl_buf1.virt_addr, g_lvgl_buf1.phy_addr, g_lvgl_buf1.size);
    }

    /* 4. LVGL 初始化 */
    lv_init();

    lv_display_t *disp = lv_display_create(g_lvgl_hor_res, g_lvgl_ver_res);

    if (g_fb_fmt != g_lvgl_fmt) {
        /* Full buffer 模式: LVGL 渲染到 DMA buf → VGS2 转换 → FB */
        lv_display_set_buffers(disp, g_lvgl_buf1.virt_addr, NULL,
                               g_lvgl_buf1.size, LV_DISPLAY_RENDER_MODE_FULL);
    } else {
        /* Direct 模式: LVGL 直接渲染到 FB (格式相同, 无需转换) */
        void *fbp = xosfb_get_fb_ptr(g_xosfb_ctx);
        size_t fb_size = g_lvgl_hor_res * g_lvgl_ver_res *
            (g_fb_fmt == XOSFB_FMT_ARGB8888 ? 4 : 2);
        lv_display_set_buffers(disp, fbp, NULL, fb_size,
                               LV_DISPLAY_RENDER_MODE_DIRECT);
    }

    lv_display_set_flush_cb(disp, lvgl_flush_v2_cb);

    return 0;
}

/* ---- 清理 ---- */
void lvgl_xosfb_v2_teardown(void)
{
    if (g_lvgl_buf1.virt_addr)
        xosfb_v2_free_dma(g_xosfb_ctx, &g_lvgl_buf1);
    if (g_xosfb_ctx)
        xosfb_v2_exit(g_xosfb_ctx);
}
```

---

## 7. 故障排查表

| 症状 | 可能原因 | 检查方法 |
|------|---------|---------|
| `xosfb_v2_init()` 返回 NULL | /dev/fb0 不存在或已被占用 | `ls -la /dev/fb0` |
| | MPP 系统未初始化或冲突 | 确认没有其他进程同时初始化 MPP |
| | VO 设备配置失败 | dmesg 查看 FH_VO_SetPubAttr 错误 |
| `xosfb_v2_blit()` 返回 -ENODEV | VGS2 硬件未就绪 | 检查 caps & XOSFB_V2_CAP_CONVERT |
| `xosfb_v2_blit()` 后画面黑屏 | src_buf 不是 DMA 内存 | 确认用了 xosfb_v2_alloc_dma() |
| | src_stride 不正确 | stride 应是像素数，不是字节数 |
| | src_fmt 与实际数据格式不匹配 | 检查 LVGL LV_COLOR_DEPTH |
| `xosfb_v2_fill_rect()` 后颜色不对 | color 值与 FB 格式不匹配 | 检查 xosfb_get_bpp()，用对应格式的颜色 |
| `xosfb_v2_alloc_dma()` 返回 -ENOMEM | VB 池耗尽 | 增加 astCommPool[0].u32BlkCnt |
| 编译找不到 `fh_*.h` | MPI 头文件路径未配置 | 检查 -I$(MPP_DIR)/include/dsp |
| 链接找不到 `FH_TDE2_Open` | 未链接 libmpi.a | 检查 LDFLAGS 包含 -lmpi |
| 运行时 "FH_TDE2_Open failed" | TDE 驱动未加载 | `lsmod \| grep tde` 或检查内核配置 |

---

## 8. 从 v1 到 v2 的标准迁移 diff

以下是修改现有 v1 集成代码的精确变更:

```diff
- #include "xosfb.h"
+ #include "xosfb_v2.h"

- xosfb_ctx_t *ctx = xosfb_init(800, 480, XOSFB_FMT_ARGB1555);
+ xosfb_ctx_t *ctx = xosfb_v2_init(800, 480, XOSFB_FMT_ARGB1555);

- xosfb_exit(ctx);
+ xosfb_v2_exit(ctx);

  /* 删掉这段 CPU 填充: */
- for (int y = 0; y < 480; y++)
-     for (int x = 0; x < 800; x++)
-         ((uint32_t*)fbp)[y * 800 + x] = 0xFF000000;
+ /* 替换为: */
+ xosfb_v2_fill_rect(ctx, 0, 0, 800, 480, 0xFF000000);

  /* flush callback 中的 memcpy: */
- for (int y = act_y1; y <= act_y2; y++) {
-     memcpy(&fbp32[location], color_p, w * 4);
-     color_p += w * 4;
- }
+ xosfb_v2_blit_desc_t desc = {
+     .src_buf = color_p, .src_w = w, .src_h = h,
+     .src_stride = LV_HOR_RES, .src_fmt = XOSFB_FMT_ARGB8888,
+     .dst_x = area->x1, .dst_y = area->y1,
+     .dst_w = w, .dst_h = h,
+ };
+ xosfb_v2_blit(ctx, &desc);

  /* Makefile 链接: */
- LDFLAGS += -lxosfb
+ LDFLAGS += -lxosfb_v2 -lmpi -lpthread -lm -ldl
```

---

## 9. 文件清单

```
xosfb/
├── xosfb.h             ← v1 公开 API (依赖)
├── xosfb.c             ← v1 实现 (依赖, 被 v2 复用)
├── Makefile            ← v1 编译
├── libxosfb.a          ← v1 静态库
│
├── xosfb_v2.h          ← v2 公开 API ★ 工程需 include
├── xosfb_v2.c          ← v2 实现 ★ 工程需编译
├── Makefile_v2         ← v2 编译脚本
├── xosfb_v2.md         ← 本文档 ★ AI agent 参考
└── libxosfb_v2.a       ← v2 静态库 (含 xosfb.o + xosfb_v2.o + libmpi.a)
```

**推荐分发方式**: 提供 `libxosfb_v2.a` + `xosfb_v2.h` + `xosfb.h` 三个文件。

---

## 10. v1 API 保留说明

以下 v1 API **完全可用且线程安全**，在 v2 context 上直接调用:

```c
void        *xosfb_get_fb_ptr(xosfb_ctx_t *ctx);
int          xosfb_get_line_length(xosfb_ctx_t *ctx);
void         xosfb_get_resolution(xosfb_ctx_t *ctx, int *w, int *h);
int          xosfb_get_bpp(xosfb_ctx_t *ctx);
xosfb_pixel_format_t xosfb_get_pixel_format(xosfb_ctx_t *ctx);
void         xosfb_pan_display(xosfb_ctx_t *ctx);
void         xosfb_show(xosfb_ctx_t *ctx, int enable);
```

这些函数不涉及硬件加速，仅操作 FB mmap 和 ioctl，与 v2 的 TDE2/VGS2 操作互不冲突。

---

## 11. 参考链接

| 资源 | 路径 |
|------|------|
| v2 头文件 | `xosfb_v2.h` |
| v2 实现 | `xosfb_v2.c` |
| v1 头文件 | `xosfb.h` |
| TDE2 MPI 头 | `$(MPP_DIR)/include/dsp/fh_tde_mpi.h` |
| VGS2 MPI 头 | `$(MPP_DIR)/include/dsp/fh_vgs2_mpi.h` |
| VB MPI 头 | `$(MPP_DIR)/include/dsp/fh_vb_mpi.h` |
| SYS MPI 头 | `$(MPP_DIR)/include/dsp/fh_system_mpi.h` |
| FB ioctl 定义 | `$(MPP_DIR)/drv_include/fb_drv_ioc.h` |
| qm10xd 完整显示框架 | `../../../core/board/generic/qm10xd/` |
