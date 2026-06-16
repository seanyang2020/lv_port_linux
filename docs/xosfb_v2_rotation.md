# XOSFB-V2 Rotation 180° Optimization Notes

## Architecture

```
LV_ROTATION=180 pipeline:

  LVGL widget   ──render──→  cached draw_buf    ──memcpy──→  DMA buffer
  (CPU SW)                   (lv_malloc, fast)   (per dirty)  (uncached)

                                                              │ VGS2 rotate_blit
                                                              │ (per dirty area, hardware)
                                                              ▼
                             Display  ←──pan──  Framebuffer
```

### Buffer layout

| Buffer | Allocator | Memory type | Size | Purpose |
|--------|-----------|-------------|------|---------|
| `draw_buf` | `lv_malloc` | **cached** | 4 MB | LVGL render target |
| `dma` | `xosfb_v2_alloc_dma` | uncached / DMA | 4 MB | VGS2 rotate_blit source |

### Flush sequence (per dirty area)

1. **memcpy** dirty area `draw_buf → dma` (bulk copy, CPU)
2. **VGS2 rotate_blit** dirty area `dma → fb` at 180° position (hardware)
3. **xosfb_pan_display** on last flush only

### Memory overhead

| | Rotation (180°, FULL) | No rotation (DIRECT) |
|---|---|---|
| **Render mode** | FULL | DIRECT |
| **Render target** | `lv_malloc` (~4 MB, cached) | FB mmap (0 extra) |
| **VGS2 source** | DMA buffer (~4 MB, uncached) | N/A |
| **Total extra** | **实测 ~6 MB** | **0** |
| **Render path** | LVGL→cached→memcpy→DMA→VGS2→FB | LVGL→FB (zero-copy) |

Without rotation, DIRECT mode renders zero-copy to the FB — no extra
buffers needed. VGS2 is not involved in the rendering pipeline.

#### 实测 ~6 MB 而非理论 ~8 MB 的原因

理论上两个 buffer 各 800×1280×4 ≈ 3.9 MB，合计 ~8 MB。实际测量增加约 **6 MB**：

1. **DMA buffer 来自 MMZ/VB 预分配池**：`xosfb_v2_alloc_dma` 从 MPP 初始化时
   预分配的 VB 池中划出，不消耗系统 DRAM。启动日志中
   `FH_VB_SetConf failed, MPP may already be initialized` 表明 MPP 已占用
   这部分内存，DMA 分配只是从现有池中切分。

2. **cached buffer 来自系统 malloc**：`lv_malloc` → `malloc(4 MB)`，这是唯一
   新增的系统内存消耗（本项目使用 `LV_STDLIB_CLIB`，直接调用标准 C `malloc`）。

3. **剩余 ~2 MB**：DMA buffer 的 CPU 虚拟地址映射（页表）、xosfb_v2 内部分配
   开销等，合计约 2 MB。

总增长 = 4 MB（cached, system heap）+ 2 MB（DMA 映射 + 开销）= **~6 MB**。

#### Why two buffers?

- **Cached buffer cannot be eliminated**: LVGL must render to cached memory
  for acceptable performance. Rendering to uncached DMA takes ~192ms/full-screen
  vs ~10ms for cached. The 4 MB is the minimum viable render target.

- **DMA buffer cannot be eliminated**: VGS2 `rotate_blit` requires a source
  buffer in MMZ/DMA memory pool. The FB physical address is NOT in this pool
  (confirmed by `-EINVAL` from VGS2). Without a separate DMA buffer, we cannot
  use VGS2 hardware rotation at all — would force full-CPU rotation at ~119ms
  per full screen.

- **Could merge the two**: No. The draw buffer needs to be cached (CPU writes);
  the DMA buffer needs to be uncached (VGS2 reads). Same memory cannot be both
  cached and uncached simultaneously.

**Impact**: +8 MB DRAM usage when `LV_ROTATION` is set. Acceptable on qm10xd
platforms (≥128 MB RAM). If memory is critically tight, the non-rotation
DIRECT path uses 0 extra memory.

### Why two buffers?

VGS2 `rotate_blit` requires a DMA (physically contiguous, MMZ-pool) source buffer.
The DMA buffer is **uncached** from CPU perspective.
LVGL writes pixel-by-pixel to the render target during widget drawing.
Scattered pixel writes to uncached memory are extremely slow.

**Solution**: LVGL renders to a cached `lv_malloc` buffer, then a single bulk
`memcpy` transfers the dirty area to the DMA buffer. The bulk memcpy is much
faster than scattered pixel writes because the CPU can batch uncached writes
via write-combine buffers.

## Performance

Test platform: qm10xd, 800×1280 ARGB8888, LV_ROTATION=180.

| Scene | Without cached buf | With cached buf | Improvement |
|-------|-------------------|-----------------|-------------|
| Static (small dirty, ~34 Kpx/f) | ~4 FPS | ~25 FPS | 6× |
| Analytics (full-screen, ~1024 Kpx/f) | ~3-4 FPS | ~5-7 FPS | 1.7× |

### Per-phase breakdown (Analytics, full-screen, 6 FPS, ~155 ms/frame)

| Phase | Time/frame | % |
|-------|-----------|------|
| LVGL widget rendering to cached buffer | ~135 ms | 87% |
| memcpy cached → DMA | ~7 ms | 4% |
| VGS2 rotate_blit | ~8 ms | 5% |
| xosfb_pan_display | ~6 ms | 4% |

**Conclusion**: Display driver overhead (memcpy + VGS2 + pan ≈ 21 ms) is minimal.
The bottleneck is LVGL CPU software widget rendering at ~135 ms per full-screen
redraw. Hardware-accelerated widget drawing (TDE2) is needed for further gains.

### Why full-screen rendering is slow

The Analytics tab has continuously-updating arc widgets that invalidate the
entire screen every frame. LVGL must re-render all widgets via CPU software
routines. On this ARM platform (no NEON, soft-float), the pixel-by-pixel
rendering of arcs, gradients, and anti-aliased edges is expensive.

## Key Design Decisions

### 1. `lv_display_set_rotation` for touch only, not matrix rotation

`lv_display_set_rotation(180)` handles touch coordinate transformation
automatically. We do NOT use `lv_display_set_matrix_rotation` because:

- `set_matrix_rotation` triggers LVGL's software rotation path, transforming
  every widget coordinate → kills FPS
- Per `lv_refr.c:897`: "In direct mode and full mode the buffer area is
  always the whole screen, not considering rotation"
- Without matrix_rotation, render coordinates are unaffected

### 2. VGS2 partial per-area rotation instead of full-FB

The display shows FB at 180° orientation. Only dirty areas change between
frames. VGS2 `rotate_blit` rotates only the dirty area from DMA to FB.
Unchanged areas stay at correct 180° positions from previous frame.

### 3. Cached buffer instead of rendering directly to DMA

| Approach | Full-screen render time | Reason |
|----------|------------------------|--------|
| LVGL → uncached DMA | ~192 ms | Scattered pixel writes to uncached |
| LVGL → cached + memcpy | ~135 ms | Cached writes + bulk uncached copy |

The bulk memcpy benefits from CPU write-combine buffering.

### 4. `LV_XOSFB_DEBUG` env var for diagnostics

```bash
LV_XOSFB_DEBUG=1   # per-second summary
LV_XOSFB_DEBUG=2   # per-second summary + per-frame detail
LV_XOSFB_DEBUG=0   # off (default)
```

Level 1 output:
```
XOSFB-V2: 25 f/s avg=40000 us/f | memcpy:5000 vgs2:8000 pan:6000 us
```

This clearly shows whether the bottleneck is in:
- `memcpy` (cached→DMA copy) — CPU bound
- `vgs2` (hardware rotation) — VGS2 bound
- LVGL widget rendering = `avg_frame - (memcpy + vgs2 + pan)` — widget rendering bound

## Env Var Reference

| Variable | Values | Description |
|----------|--------|-------------|
| `LV_ROTATION` | 0, 90, 180, 270 | Rotation angle (90/270 need VGS2) |
| `LV_XOSFB_DEBUG` | 0, 1, 2 | Debug log level |
| `LV_XOSFB_WIDTH` | pixels | Override display width (default: 800) |
| `LV_XOSFB_HEIGHT` | pixels | Override display height (default: 1280) |

---

# TDE2 Acceleration — Failed Attempts & Lessons

TDE2 `xosfb_v2_fill_rect` can hardware-accelerate solid-color fills.
Since fills are the most common LVGL draw task, accelerating them could
significantly reduce the 135 ms widget rendering time.

## Attempt 1: TDE2 draw unit + DIRECT mode + VGS2 FB→FB rotate

### Architecture
```
LVGL phase:
  TDE2 draw unit → fill_rect → FB 0° (hardware, fast)
  LVGL SW → arc/label → FB 0°

Flush phase:
  VGS2 rotate_blit FB 0° → FB 180° (hardware, no DMA buffer needed)
```

### Failure: VGS2 rejects FB as source

`xosfb_v2_rotate_blit` with FB physical address as source returns `-22` (EINVAL).
VGS2 requires the source buffer to be in the MMZ DMA memory pool. The FB
(allocated by the display driver via `/dev/fb0` mmap) is not in this pool.

### CPU fallback complexity

The VGS2 failure triggered a CPU partial rotate fallback. This introduced
multiple issues:

1. **Source/destination overlap**: For 180° rotation of a full-screen area,
   source (0°) and destination (180°) positions overlap. A simple COPY
   corrupts data. Requires in-place SWAP+REVERSE algorithm.

2. **Stale data in 0° scratch area**: SWAP modifies both source and dest.
   Between frames, the 0° "staging area" accumulates stale 180° data that
   leaks into subsequent partial renders → visual artifacts.

3. **COPY for non-overlapping areas**: For partial areas away from screen
   center, COPY (read 0° → write 180°) preserves the 0° scratch area.
   But this requires overlap detection, adding complexity.

4. **Uncached FB memory**: CPU rotation reads/writes the FB directly,
   which is uncached. Full-screen rotate takes ~119 ms. Combined with
   TDE2 fill acceleration, this might still be worth it, but the
   complexity was excessive.

### Key takeaways

1. **VGS2 source must be MMZ/DMA memory** — FB physical address is not accepted.
   A separate DMA buffer is always needed for VGS2 operations.

2. **DIRECT mode + in-place rotation is fundamentally hard**: The FB serves
   dual purpose as LVGL render target and display output. Keeping 0° scratch
   area and 180° display area correct across frames requires careful state
   tracking.

3. **CPU rotation on uncached FB is slow** regardless of algorithm.

## Recommended Future Approach

1. **Validate TDE2 without rotation first**: DIRECT mode + TDE2 fill + no rotation.
   Prove that TDE2 significantly reduces LVGL render time before adding rotation
   complexity.

2. **TDE2 + cached buffer + VGS2 DMA→FB**: Keep the working cached buffer
   architecture. Implement TDE2 fill operations that target the LVGL draw
   buffer (not FB). This requires TDE2 to write to an arbitrary physical
   address — either via a new API or by remapping the draw buffer as TDE2 target.

3. **Integrate at LVGL draw task level**: Instead of a custom draw unit,
   override LVGL's software fill function to use TDE2 when the target
   buffer supports it. This is simpler than a full draw unit and keeps
   compatibility with the existing pipeline.
