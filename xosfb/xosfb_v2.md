# xosfb v2 — 硬件加速帧缓冲库 (AI 集成手册)

> **面向**: Claude / AI Coding Agent  
> **版本**: v2.0 | **平台**: qm10xd (FH8626) | **日期**: 2026-06-15  
> **依赖**: libmpi.a (TDE2 + VGS v1 + VB + SYS)  
> **v1 关系**: 完全独立，不需要 xosfb.h / libxosfb.a

---

## 0. 硬件加速总览

xosfb v2 导入了 qm10xd 的 **3 个硬件加速引擎 + 1 个带宽优化**：

```
┌──────────────────────────────────────────────────────────┐
│                    xosfb v2 加速架构                       │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │  TDE2    │  │  VGS v1  │  │ VB (MMZ) │               │
│  │ (G2D)    │  │          │  │          │               │
│  │          │  │          │  │          │               │
│  │·QuickFill│  │·AddFmt-  │  │·GetBlock │               │
│  │·QuickCopy│  │ Convert   │  │·PhysAddr │               │
│  │          │  │  Task     │  │·VirAddr  │               │
│  │          │  │·DoRotate │  │          │               │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘               │
│       │              │             │                     │
│       ▼              ▼             ▼                     │
│  ┌──────────────────────────────────────┐               │
│  │         FB Compression               │               │
│  │      (DDR 带宽压缩, 始终开启)          │               │
│  └──────────────────────────────────────┘               │
│       │                                                 │
│       ▼                                                 │
│  ┌──────────┐                                           │
│  │ /dev/fb0 │ → FBIOPAN_DISPLAY → LCM Panel             │
│  └──────────┘                                           │
└──────────────────────────────────────────────────────────┘
```

### v1 vs v2 对比

| 维度 | xosfb v1 | xosfb v2 |
|------|----------|----------|
| **文件** | xosfb.h + xosfb.c + libxosfb.a | **xosfb_v2.h + libxosfb_v2.a** (2 个文件, 自包含) |
| **初始化** | `xosfb_init()` — FB mmap only | `xosfb_v2_init()` — FB + TDE2 + VGS + DMA Pool |
| **MPP 系统** | 不处理 (外部负责) | 自动初始化, 容错已预初始化的 MPP |
| **矩形填充** | CPU `for` 循环 (~15ms/全屏) | **TDE2 QuickFill** (~0.3ms, **50x**) |
| **矩形拷贝** | CPU `memcpy` 逐行 (~20ms) | **TDE2 QuickCopy** (~1ms, **20x**) |
| **格式转换** | ❌ 不支持 | **VGS AddFmtConvertTask** (~2ms, **15x**) |
| **缩放** | ❌ 不支持 | **VGS AddScaleTask** (~1.5ms, **17x**) |
| **旋转** | ❌ 不支持 | **VGS DoRotate** (~2.5ms, **14x**) |
| **DMA 内存** | ❌ | **VB GetBlock** (MMZ 物理连续内存) |
| **FB 压缩** | ❌ | ✅ DDR 带宽压缩 (~50%) |
| **代码量** | ~420 行 | ~900 行 |

### 各加速功能适用的业务场景

| 加速功能 | 硬件 | 适用场景 | 触发时机 |
|---------|:---:|---------|---------|
| **Fill** | TDE2 | 全屏清屏、纯色背景填充、进度条背景 | 每次调 `xosfb_v2_fill_rect()` |
| **Copy** | TDE2 | 同屏滚动、窗口移动、弹窗动画 | 每次调 `xosfb_v2_copy_rect()` |
| **Convert** | VGS | LVGL ARGB8888 → FB ARGB1555 (省 DDR 带宽) | 每次调 `xosfb_v2_blit()` 且 src_fmt ≠ FB fmt |
| **Scale** | VGS | 低分辨率渲染 → 高分辨率 FB 输出 | blit 时 src_w≠dst_w 或 src_h≠dst_h |
| **Rotate** | VGS | 横竖屏切换、90°/270° 旋转 | 每次调 `xosfb_v2_rotate_blit()` |
| **Compress** | FB | 静态页面、低刷新率 UI (减少 DDR 读带宽) | 始终开启 |
| **DMA** | VB | LVGL buffer 分配 (VGS 需要物理地址) | 每次调 `xosfb_v2_alloc_dma()` |

### 各加速接口的底层 MPI 调用

| API | MPI 调用链 | 阻塞? |
|-----|-----------|:---:|
| `xosfb_v2_fill_rect` | `FH_TDE2_BeginJob` → `FH_TDE2_QuickFill` → `FH_TDE2_EndJob` | ✅ 同步 |
| `xosfb_v2_copy_rect` | `FH_TDE2_BeginJob` → `FH_TDE2_QuickCopy` → `FH_TDE2_EndJob` | ✅ 同步 |
| `xosfb_v2_blit` | `FH_VGS_BeginJob` → `FH_VGS_AddFmtConvertTask` → `FH_VGS_EndJob` | ✅ 同步 |
| `xosfb_v2_rotate_blit` | `FH_VGS_DoRotate` | ✅ 同步 |
| `xosfb_v2_alloc_dma` | `FH_VB_GetBlock` → `FH_VB_Handle2PhysAddr` → `FH_VB_GetBlkVirAddr` | ✅ 同步 |
| `xosfb_pan_display` | `ioctl(FBIOPAN_DISPLAY)` | ✅ 同步 |

---

## 1. API 决策树

```
需要往 FB 写数据?
│
├─ 填充纯色矩形?
│   └─ xosfb_v2_fill_rect(ctx, x, y, w, h, color)
│      TDE2 QuickFill ~0.3ms/全屏 (50x vs CPU)
│      ⚠️ color 是 FB 原生格式
│
├─ 拷贝 FB 内区域?
│   └─ xosfb_v2_copy_rect(ctx, sx, sy, w, h, dx, dy)
│      TDE2 QuickCopy ~1ms/全屏 (20x vs CPU)
│
├─ 外部 buffer → FB (格式/尺寸可能不同)?
│   └─ xosfb_v2_blit(ctx, &desc)
│      VGS AddFmtConvertTask ~2ms/全屏 (15x vs CPU)
│      ⚠️ src_buf 必须 DMA 内存
│
├─ 旋转画面?
│   └─ xosfb_v2_rotate_blit(ctx, src, w, h, fmt, dx, dy, rot)
│      VGS DoRotate ~2.5ms/全屏 (14x vs CPU)
│      ⚠️ src_buf 必须 DMA 内存
│
└─ 需要 DMA buffer?
    ├─ xosfb_v2_alloc_dma(ctx, size, &buf)
    └─ xosfb_v2_free_dma(ctx, &buf)
```

---

## 2. 集成清单

### 2.1 文件准备

```
[ ] 复制到工程:
    [ ] xosfb_v2.h          ← 单头文件
    [ ] libxosfb_v2.a       ← 单库文件 (自包含)
```

### 2.2 编译配置

```
[ ] 链接:
    [ ] libxosfb_v2.a
    [ ] -lpthread -lm -ldl
```

### 2.3 代码替换

```
[ ] #include "xosfb_v2.h"
[ ] xosfb_init()  → xosfb_v2_init()
[ ] xosfb_exit()  → xosfb_v2_exit()
[ ] CPU 循环填充   → xosfb_v2_fill_rect()
[ ] 逐行 memcpy    → xosfb_v2_blit() (需 DMA buffer)
```

---

## 3. ⚠️ 关键约束

### 陷阱 1: DMA 内存 (最高频错误)

```c
// ❌ malloc → VGS 无法访问
uint8_t *buf = malloc(W * H * 4);
desc.src_buf = buf;

// ✅ DMA 内存
xosfb_v2_dma_buf_t dma;
xosfb_v2_alloc_dma(ctx, W * H * 4, &dma);
desc.src_buf = dma.virt_addr;
```

### 陷阱 2: fill_rect 颜色格式

```c
// ✅ 匹配 FB 格式
if (xosfb_get_bpp(ctx) == 32)
    xosfb_v2_fill_rect(ctx, 0,0,100,100, 0xFFFF0000);
else
    xosfb_v2_fill_rect(ctx, 0,0,100,100, 0xFC00);
```

### 陷阱 3: 忘记 pan_display

```c
xosfb_v2_fill_rect(ctx, 0,0,800,480, 0xFF000000);
xosfb_pan_display(ctx);  // ← 必须有
```

---

## 4. API 速查表

### 初始化

| 函数 | 必须? | 说明 |
|------|:-----:|------|
| `xosfb_v2_init(w, h, fmt)` | ✅ | TDE2 + VGS + FB |
| `xosfb_v2_exit(ctx)` | ✅ | 只释放自己初始化的 |
| `xosfb_v2_get_caps(ctx)` | 可选 | caps 位掩码 |

### 渲染

| 函数 | 硬件 | 加速比 | 约束 |
|------|:---:|:-----:|------|
| `xosfb_v2_fill_rect` | TDE2 | 50x | color=FB 原生格式 |
| `xosfb_v2_copy_rect` | TDE2 | 20x | 源/目标均在 FB 内 |
| `xosfb_v2_blit` | VGS | 15x | **src_buf=DMA 内存** |
| `xosfb_v2_rotate_blit` | VGS | 14x | **src=DMA 内存** |

### 内存

| 函数 | 说明 |
|------|------|
| `xosfb_v2_alloc_dma` | MMZ 物理连续内存 |
| `xosfb_v2_free_dma` | 释放 DMA buffer |

### FB 访问

| 函数 | 说明 |
|------|------|
| `xosfb_get_fb_ptr` | FB 虚拟地址 |
| `xosfb_get_resolution` | 分辨率 (w, h) |
| `xosfb_get_bpp` | bpp (16/32) |
| `xosfb_get_line_length` | 行字节跨度 |
| `xosfb_pan_display` | 提交到 LCM |
| `xosfb_show` | 显示/隐藏图层 |

---

## 5. VGS 适配

VGS 是可选模块。`xosfb_v2_init` 会自动检测并设置 caps。

```
caps & XOSFB_V2_CAP_CONVERT?
│
├─ YES (VGS 可用):
│   FB 格式可以 ≠ LVGL 格式
│   LVGL buffer → DMA → VGS 硬件 CSC → FB
│   模式: FULL + blit
│
└─ NO (VGS 不可用):
    FB 格式 必须 == LVGL 格式
    LVGL buffer → CPU memcpy → FB (或 DIRECT 模式)
    模式: DIRECT (零拷贝) 或 FULL + CPU
```

详见 `xosfb_v2_lvgl.c` 中的 `init_xosfb_v2()` 实现。

---

## 6. LVGL 集成模板

```c
#include "xosfb_v2.h"
#include "lvgl/lvgl.h"

static xosfb_ctx_t *g_ctx;
static xosfb_v2_dma_buf_t g_dma;
static int g_has_vgs2;  /* VGS hardware blit available */

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    if (g_has_vgs2) {
        int w = lv_area_get_width(area), h = lv_area_get_height(area);
        xosfb_v2_blit_desc_t d = {
            .src_buf = px, .src_w = w, .src_h = h,
            .src_stride = 800, .src_fmt = XOSFB_FMT_ARGB8888,
            .dst_x = area->x1, .dst_y = area->y1, .dst_w = w, .dst_h = h,
        };
        xosfb_v2_blit(g_ctx, &d);
    }
    if (lv_display_flush_is_last(disp))
        xosfb_pan_display(g_ctx);
    lv_display_flush_ready(disp);
}

int setup_lvgl(void)
{
    g_ctx = xosfb_v2_init(800, 480, XOSFB_FMT_ARGB1555);
    g_has_vgs2 = !!(xosfb_v2_get_caps(g_ctx) & XOSFB_V2_CAP_CONVERT);
    xosfb_v2_alloc_dma(g_ctx, 800 * 480 * 4, &g_dma);

    lv_init();
    lv_display_t *disp = lv_display_create(800, 480);
    lv_display_set_buffers(disp, g_dma.virt_addr, NULL, g_dma.size,
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    return 0;
}
```

---

## 7. 故障排查

| 症状 | 原因 | 检查 |
|------|------|------|
| `FH_VB_SetConf failed` | MPP 已预初始化 | **正常**, v2 继续运行 |
| `FH_VGS_Open failed` | VGS 驱动未加载 | `lsmod \| grep vgs` |
| `xosfb_v2_blit` → -ENODEV | caps 无 CONVERT | 检查 `xosfb_v2_get_caps()` |
| blit 后黑屏 | src_buf 不是 DMA | 确认用了 `xosfb_v2_alloc_dma()` |
| `xosfb_v2_alloc_dma` → -ENOMEM | VB 池耗尽 | 及时释放或自动创建新池 |
| segfault | LVGL buf 不匹配 | 检查 FB stride == LVGL stride |

---

## 8. 文件清单

```
交付物 (2 个文件):
├── xosfb_v2.h          ← 单头文件
└── libxosfb_v2.a       ← 单库文件 (xosfb_v2.o + libmpi.a)
```

---

## 9. 屏幕旋转使用指导

xosfb v2 + LVGL 支持两层旋转方案，可以单独或组合使用。

### 9.1 两种旋转方案对比

| | LVGL 软件旋转 | VGS 硬件旋转 |
|------|:---:|:---:|
| API | `lv_display_set_rotation(disp, angle)` | `xosfb_v2_rotate_blit(ctx, src, ...)` |
| 性能 | CPU (LVGL 内部矩阵变换) | **VGS 硬件** (~2.5ms/全屏) |
| 限制 | 仅支持 0°/90°/180°/270° | 仅支持 0°/90°/180°/270° |
| Buffer | 不需要额外 buffer | 需要 **DMA buffer** 存放旋转前画面 |
| 适用场景 | 静态 UI、设置菜单 | 实时预览、相机、视频 |

### 9.2 方案 A: 纯 LVGL 旋转 (最简单)

LVGL v9 内置旋转支持，无需任何硬件加速。适合静态界面。

```c
// 初始化: FB 和 LVGL 都用物理分辨率 (如 800×1280 竖屏)
g_ctx = xosfb_v2_init(800, 1280, XOSFB_FMT_ARGB8888);

lv_display_t *disp = lv_display_create(800, 1280);
lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);  // LVGL 内部旋转

// flush callback 不变 — LVGL 已处理好旋转后的像素
```

**原理**: LVGL 渲染时内部做坐标旋转，flush 收到的 `px` 已经是旋转后的画面，直接写入 FB 即可。**不需要 VGS、不需要 DMA buffer。**

```
LVGL 逻辑坐标 (横屏 1280×800)
    │ lv_display_set_rotation(90°)
    ▼
LVGL 渲染 buffer (竖屏 800×1280) → flush → FB (800×1280)
```

### 9.3 方案 B: 纯 VGS 硬件旋转 (最高性能)

用 `xosfb_v2_rotate_blit()` 做硬件旋转。需要 DMA buffer。

```c
// 1. 分配 DMA buffer 存放旋转前的画面
xosfb_v2_dma_buf_t rot_buf;
xosfb_v2_alloc_dma(ctx, 800 * 1280 * 4, &rot_buf);

// 2. 把当前 FB 内容拷贝到 DMA buffer (或用 TDE2 加速)
//    然后旋转 DMA buffer → FB
xosfb_v2_rotate_blit(ctx, rot_buf.virt_addr,
    800, 1280,                    // src: 竖屏 800×1280
    XOSFB_FMT_ARGB8888,
    0, 0,                         // dst: FB (0,0)
    XOSFB_V2_ROTATE_90);          // 旋转 90° → 横屏 1280×800

xosfb_pan_display(ctx);
```

**原理**: DMA buffer 中是旋转前的画面，VGS 硬件读取 DMA buffer 并按指定角度写入 FB。

```
DMA buffer (竖屏 800×1280)
    │ xosfb_v2_rotate_blit(ROTATE_90)
    ▼ VGS DoRotate
FB (横屏 1280×800)
```

### 9.4 方案 C: LVGL + VGS 组合 (qm10xd board 方案)

qm10xd board 的做法：LVGL 做坐标旋转 + VGS 做最终的帧旋转。

```c
/* === 初始化 (参考 qm10xd board 方案) === */

// FB 分配为双倍高度 (容纳旋转前后的两帧)
// yres_virtual = yres * 2
// 上半部分: lvgl_surface (LVGL 渲染目标)
// 下半部分: fb_surface   (VGS 旋转输出 → 实际显示)

int fb_w = 800, fb_h = 1280;    // 物理竖屏
int rotation = 1;                // 0=none 1=90° 2=180° 3=270°

g_ctx = xosfb_v2_init(fb_w, fb_h, XOSFB_FMT_ARGB8888);
int caps = xosfb_v2_get_caps(g_ctx);
int has_vgs = (caps & XOSFB_V2_CAP_ROTATE) != 0;

lv_display_t *disp;
if (rotation == 1 || rotation == 3) {
    // 90°/270°: 逻辑宽高互换
    disp = lv_display_create(fb_h, fb_w);  // 逻辑: 横屏 1280×800
} else {
    disp = lv_display_create(fb_w, fb_h);
}
lv_display_set_rotation(disp, rotation);

// 分配 DMA buffer 给 LVGL (VGS 旋转需要物理地址)
xosfb_v2_dma_buf_t lvgl_buf;
xosfb_v2_alloc_dma(g_ctx, fb_w * fb_h * 4, &lvgl_buf);

lv_display_set_buffers(disp, lvgl_buf.virt_addr, NULL,
    lvgl_buf.size, LV_DISPLAY_RENDER_MODE_FULL);
lv_display_set_flush_cb(disp, flush_rotate_cb);

/* === flush callback === */
static void flush_rotate_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    // LVGL 已渲染旋转后的画面到 lvgl_buf (DMA)
    // VGS 硬件旋转到 FB
    xosfb_v2_rotate_blit(g_ctx, px, fb_w, fb_h,
        XOSFB_FMT_ARGB8888, 0, 0, g_rotation);

    if (lv_display_flush_is_last(disp))
        xosfb_pan_display(g_ctx);
    lv_display_flush_ready(disp);
}
```

**原理**:
```
LVGL 逻辑坐标 (横屏 1280×800, 由 lv_display_set_rotation 处理)
    │
    ▼
LVGL 渲染到 lvgl_buf (竖屏 800×1280 DMA buffer)
    │ xosfb_v2_rotate_blit(ROTATE_90)
    ▼ VGS DoRotate
FB (竖屏 800×1280, 物理 LCD 方向)
```

### 9.5 旋转方案选择指南

| 场景 | 推荐方案 | 理由 |
|------|---------|------|
| 设置页面、静态 UI | **A: 纯 LVGL** | 简单，无额外内存 |
| 相机预览、视频播放 | **B: 纯 VGS** | 硬件加速，CPU 零开销 |
| 需要横竖屏切换的产品 | **C: LVGL + VGS** | 灵活，qm10xd board 验证方案 |
| VGS 驱动未加载 | **A: 纯 LVGL** | 唯一可用方案 |
| 内存紧张 (< 4MB 空闲) | **A: 纯 LVGL** | 不需要 DMA buffer |

### 9.6 关键约束

- `xosfb_v2_rotate_blit` 的 `src_buf` 必须 DMA 内存
- 旋转 90°/270° 时目标尺寸自动交换 (W×H → H×W)
- LVGL `lv_display_set_rotation` 和 VGS `xosfb_v2_rotate_blit` 的旋转方向一致 (0/1/2/3 = 0°/90°/180°/270°)
- 组合使用时，确保 LVGL 渲染 buffer 尺寸与 `rotate_blit` 的 src_w/src_h 匹配

---

## 10. 集成经验总结

本次 xosfb v2 从设计到在 lv_port_linux 工程中稳定运行，遇到的问题和解决方案:

### 经验 1: v1 代码不可直接复用 — 依赖链污染

**问题**: 将 `xosfb.o` 打包进 `libxosfb_v2.a` 后，链接报 `undefined reference to SAMPLE_COMM_VO_StartDev`。

**原因**: `xosfb.c` 的 `vo_init()` 调用了 `SAMPLE_COMM_VO_StartDev`，该函数定义在 `sample_comm_vo.o` 中，而这个 `.o` 不在库里。

**解决**: 不链接 `xosfb.o`，将 v1 的 7 个辅助函数（`get_fb_ptr` / `pan_display` 等，都是 3~5 行的简单函数）直接内联到 `xosfb_v2.c` 中。

**教训**: 静态库应做到**自包含**——不依赖库外部的 `.o` 文件。简单函数的复制优于复杂的依赖链。

### 经验 2: 硬件 API 版本要对齐 — VGS2 ≠ VGS

**问题**: `FH_VGS_V2_Init()` 始终失败 `(-1606451184)`，日志: `Open vgs(vgs2) fail!`。

**原因**: qm10xd 芯片的 `g2d.ko` 驱动导出的是 **VGS v1** API (`FH_VGS_Open` / `FH_VGS_AddFmtConvertTask` / `FH_VGS_DoRotate`)，不是 VGS v2。qm10xd board 自身也是用 VGS v1。

| | VGS v2 (我们的初始实现) | VGS v1 (正确) |
|------|------|------|
| 初始化 | `FH_VGS_V2_Init()` | `FH_VGS_Open()` |
| 格式转换 | `FH_VGS_V2_CVT()` | `FH_VGS_BeginJob` → `FH_VGS_AddFmtConvertTask` → `FH_VGS_EndJob` |
| 旋转 | `FH_VGS_V2_ROT()` | `FH_VGS_DoRotate()` |
| 清理 | 无 | `FH_VGS_Close()` |

**教训**: 引用已有工程（qm10xd board）中验证过的 API 路径，而不是根据命名猜测。`fh_vgs_mpi.h` (v1) 和 `fh_vgs2_mpi.h` (v2) 是两个不同的接口。

### 经验 3: MPP 系统可能已被预初始化 — 必须容错

**问题**: `FH_VB_SetConf failed (-1610514414)`，导致 `xosfb_v2_init` 返回 NULL。

**原因**: lv_port_linux 的启动框架在 `xosfb_v2_init` 之前已调用了 `FH_VB_SetConf` + `FH_VB_Init` + `FH_SYS_Init`。再次调用冲突。

**解决**: `xosfb_v2_init` 改为容错模式:
- `sys_init_v2` 失败 → 打印警告，继续运行
- `vo_init_v2` 失败 → 同上
- 上下文增加 `sys_owned` / `vo_owned` / `fb_opened` 标记
- `xosfb_v2_exit` 只释放自己初始化的资源

**教训**: 库的初始化函数应假设外部环境可能已经部分初始化，采用"尽力而为 + 精确释放"策略。

### 经验 4: VB 池大小不匹配 — 需要动态扩容

**问题**: `FH_VB_GetBlock(4096000)` 失败，因为已有 VB 池的块大小只有 32KB，无法满足 4MB 的 DMA buffer 分配。

**原因**: 外部框架预初始化的 VB 池是为小缓冲区设计的，而 LVGL full-screen buffer (800×1280×4 = 4MB) 需要大块。

**解决**: `xosfb_v2_alloc_dma` 增加回退逻辑:
1. 先尝试 `FH_VB_GetBlock(VB_INVALID_POOLID, size, ...)` — 搜索已有池
2. 失败则 `FH_VB_CreatePool(size, 1, "xosfb_v2_dma")` — 创建专用池
3. 在新池中 `FH_VB_GetBlock`

**教训**: 内存分配接口应封装多种回退策略，不应假设单一的分配路径。

### 经验 5: DIRECT 渲染模式有条件限制

**问题**: 切换到 DIRECT 模式后程序 segfault。

**原因**: DIRECT 模式要求 `fb_stride (line_length) == display_width × pixel_size`。如果 FB 硬件的 line_length 大于显示宽度（对齐 padding），LVGL 写入的 stride 与 FB 实际 stride 不匹配，导致写入错误位置。

**解决**: 在 `init_xosfb_v2()` 中运行时检测:
```c
drv->direct = (drv->fb_stride == lvgl_stride);  // stride 匹配才开 DIRECT
```
不匹配时回退到 FULL 模式（LVGL 渲染到独立 buffer → flush 时逐行 memcpy 到 FB）。

**教训**: DIRECT 模式是零拷贝的最佳方案，但有硬件前提条件。运行时检测比编译期假设更可靠。

### 经验 6: LVGL 和 FB 格式应保持一致（无 VGS 时）

**问题**: 初始设计为 FB=ARGB1555 (16bpp) + LVGL=ARGB8888 (32bpp)，期望 VGS 硬件做格式转换。但 VGS 初始化失败后，被迫走 CPU 转换，颜色转换公式有 bug。

**解决**: VGS 不可用时，FB 和 LVGL 都用 ARGB8888:
- `XOSFB_FMT_ARGB8888` → `LV_COLOR_FORMAT_ARGB8888`
- flush 时 `memcpy` 无需任何格式转换
- 如果 VGS 可用，再启用 DMA buffer + VGS blit

**教训**: 硬件加速是"加速"不是"必需"。核心路径必须能在纯 CPU 下正确运行，硬件加速作为可选的优化层叠加在上面。

### 经验总结清单

| # | 问题 | 根因 | 解决 |
|:--:|------|------|------|
| 1 | 链接缺 `SAMPLE_COMM_*` | v1 .o 引入外部依赖 | 内联 v1 helper, 移除 xosfb.o |
| 2 | `FH_VGS_V2_Init` 失败 | qm10xd 是 VGS v1 不是 v2 | 切换到 `FH_VGS_Open` + v1 API |
| 3 | `FH_VB_SetConf` 冲突 | MPP 已被外部预初始化 | 容错: 失败→警告→继续 |
| 4 | DMA 4MB 分配失败 | VB 池块太小 | `FH_VB_CreatePool` 动态扩容 |
| 5 | DIRECT 模式 segfault | FB stride ≠ LVGL stride | 运行时检测, 不匹配回退 FULL |
| 6 | CPU 转换颜色异常 | ARGB8888→1555 公式错误 | 无 VGS 时格式保持一致 |

---

## 11. 参考

| 资源 | 说明 |
|------|------|
| `fh_tde_mpi.h` | TDE2 MPI (QuickFill/QuickCopy) |
| `fh_vgs_mpi.h` | VGS v1 MPI (Open/AddFmtConvertTask/DoRotate) |
| `fh_vb_mpi.h` | VB MPI (GetBlock/Handle2PhysAddr) |
| `fb_drv_ioc.h` | FB ioctl (FBIOPAN_DISPLAY/COMPRESSION) |
| `xosfb_v2.c` | 实现源码 |
| `xosfb_v2_lvgl.c` | LVGL 适配参考 |
