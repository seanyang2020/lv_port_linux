/**
 * @file xosfb_v2_lvgl.c — LVGL display driver for xosfb v2
 *
 * Optimized for TDE2 hardware acceleration (fill/copy).
 * VGS2 (blit/rotate) is optional — falls back gracefully when unavailable.
 *
 * Two rendering paths:
 *   VGS2 available → DMA buffer + hardware CSC blit (best)
 *   VGS2 absent    → DIRECT render to FB (zero-copy, no memcpy flush)
 *
 * Required:  xosfb_v2.h + libxosfb_v2.a
 */
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lvgl/lvgl.h"
#include "xosfb_v2.h"
#include "../simulator_util.h"
#include "../backends.h"

#if LV_USE_LINUX_FBDEV

typedef struct {
    xosfb_ctx_t       *ctx;
    xosfb_v2_dma_buf_t dma;       /* LVGL draw buffer (FULL mode) */
    xosfb_v2_dma_buf_t fb_dma;    /* FB wrapper for rotate_blit */
    uint8_t           *draw_buf;
    int                px_size;
    int                fb_stride;
    int                w, h;
    bool               use_vgs2;
    bool               hw_rotate;
    int                hw_deg;
    bool               direct;
    /* debug: LV_XOSFB_DEBUG bitmask: 1=summary 2=per-frame */
    struct {
        int             level;          /* parsed once from env */
        uint32_t        frame_cnt;
        uint32_t        flush_cnt;
        uint32_t        total_dirty_px;
        uint32_t        memcpy_us;
        uint32_t        vgs2_us;
        uint32_t        pan_us;
        uint32_t        frame_us;       /* sum of all frame times */
        struct timespec frame_start;    /* render_start_cb timestamp */
        struct timespec last_log;
    } dbg;
} xosfb_v2_drv_t;

static lv_display_t *init_xosfb_v2(void);
static void run_loop_xosfb_v2(void);
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px);
static void del_cb(lv_event_t *e);
static void render_start_cb(lv_event_t *e);
static uint32_t tick_get_cb(void);

/* usleep: need explicit decl, lvgl headers may reset _POSIX_C_SOURCE */
int usleep(unsigned int);

static char *backend_name = "XOSFB-V2";
static int  g_hw_rotation = 0;
static int  g_disp_w = 800, g_disp_h = 1280;

/* Touch coordinate transform — call from evdev handler before lv_indev_read.
 * Only needed for VGS hardware rotation. Software rotation is auto-handled. */
void xosfb_v2_transform_touch(int *x, int *y)
{
    if (g_hw_rotation == 0) return;
    int ox = *x, oy = *y;
    if (g_hw_rotation == 90)      { *x = oy;         *y = g_disp_w - ox; }
    else if (g_hw_rotation == 180){ *x = g_disp_w - ox; *y = g_disp_h - oy; }
    else if (g_hw_rotation == 270){ *x = g_disp_h - oy; *y = ox;           }
}

int backend_init_xosfb(backend_t *b)
{
    b->handle->display = malloc(sizeof(display_backend_t));
    LV_ASSERT_NULL(b->handle->display);
    b->handle->display->init_display = init_xosfb_v2;
    b->handle->display->run_loop     = run_loop_xosfb_v2;
    b->name = backend_name;
    b->type = BACKEND_DISPLAY;
    return 0;
}

static uint32_t tick_get_cb(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}


static int debug_parse_level(void)
{
    const char *s = getenv("LV_XOSFB_DEBUG");
    return s ? atoi(s) : 0;
}

/* Microseconds between two timestamps */
static inline uint32_t debug_elapsed(const struct timespec *t0,
                                     const struct timespec *t1)
{
    return (uint32_t)((t1->tv_sec - t0->tv_sec) * 1000000
                    + (t1->tv_nsec - t0->tv_nsec) / 1000);
}

/* Record flush counters (always called for rotate path) */
static inline void debug_flush_add(xosfb_v2_drv_t *drv,
                                   int rw, int rh)
{
    if (!drv->dbg.level) return;
    drv->dbg.flush_cnt++;
    drv->dbg.total_dirty_px += (uint32_t)(rw * rh);
}

/* End-of-frame: update frame counter + total time, maybe log */
static void debug_frame_end(xosfb_v2_drv_t *drv)
{
    int lv = drv->dbg.level;
    if (!lv) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    drv->dbg.frame_cnt++;
    drv->dbg.frame_us += debug_elapsed(&drv->dbg.frame_start, &now);

    if (lv >= 2) {
        uint32_t frame = debug_elapsed(&drv->dbg.frame_start, &now);
        LV_LOG_USER("XOSFB-V2: frame #%u %u us | memcpy:%u vgs2:%u pan:%u | %u fl %u Kpx",
                    drv->dbg.frame_cnt, frame,
                    drv->dbg.memcpy_us, drv->dbg.vgs2_us, drv->dbg.pan_us,
                    drv->dbg.flush_cnt,
                    drv->dbg.total_dirty_px / 1000);
    }

    /* Reset per-frame counters */
    drv->dbg.memcpy_us = 0;
    drv->dbg.vgs2_us   = 0;
    drv->dbg.pan_us    = 0;
    drv->dbg.flush_cnt = 0;
    drv->dbg.total_dirty_px = 0;
}

/* Per-second summary dump + reset */
static void debug_summary_try(xosfb_v2_drv_t *drv)
{
    int lv = drv->dbg.level;
    if (lv < 1) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double dt = (now.tv_sec - drv->dbg.last_log.tv_sec)
              + (now.tv_nsec - drv->dbg.last_log.tv_nsec) / 1e9;
    if (dt < 1.0) return;

    uint32_t n = drv->dbg.frame_cnt;
    LV_LOG_USER("XOSFB-V2: %u f/s avg=%u us/f | memcpy:%u vgs2:%u pan:%u us",
                n, n ? drv->dbg.frame_us / n : 0,
                drv->dbg.memcpy_us, drv->dbg.vgs2_us, drv->dbg.pan_us);

    drv->dbg.frame_cnt = 0;
    drv->dbg.frame_us  = 0;
    drv->dbg.memcpy_us = 0;
    drv->dbg.vgs2_us   = 0;
    drv->dbg.pan_us    = 0;
    drv->dbg.last_log  = now;
}


static void run_loop_xosfb_v2(void)
{
    while (true) {
        uint32_t idle_time = lv_timer_handler();
        usleep(idle_time * 1000);
    }
}


static lv_display_t *init_xosfb_v2(void)
{
    const char *ew = getenv("LV_XOSFB_WIDTH"), *eh = getenv("LV_XOSFB_HEIGHT");
    int w = ew ? atoi(ew) : 800, h = eh ? atoi(eh) : 1280;

    lv_tick_set_cb(tick_get_cb);

    xosfb_v2_drv_t *drv = lv_malloc_zeroed(sizeof(xosfb_v2_drv_t));
    LV_ASSERT_MALLOC(drv);
    if (!drv) return NULL;

    drv->ctx = xosfb_v2_init(w, h, XOSFB_FMT_ARGB8888);
    if (!drv->ctx) { LV_LOG_ERROR("xosfb_v2_init failed"); lv_free(drv); return NULL; }
    drv->w = w; drv->h = h;
    drv->px_size = xosfb_get_bpp(drv->ctx) / 8;
    drv->fb_stride = xosfb_get_line_length(drv->ctx);

    uint32_t caps = xosfb_v2_get_caps(drv->ctx);
    drv->use_vgs2 = (caps & XOSFB_V2_CAP_CONVERT) != 0;
    int lvgl_stride = w * drv->px_size;
    drv->direct = (drv->fb_stride == lvgl_stride); /* stride match → safe for DIRECT */

    LV_LOG_INFO("XOSFB-V2: %dx%d bpp=%d caps=0x%x VGS2=%d fb_stride=%d lvgl_stride=%d direct=%d",
                w, h, drv->px_size * 8, caps, drv->use_vgs2,
                drv->fb_stride, lvgl_stride, drv->direct);

    /* ---- Rotation check ---- */
    const char *rot_env = getenv("LV_ROTATION");
    int rot_deg = rot_env ? atoi(rot_env) : 0;
    if (rot_deg != 0 && rot_deg != 90 && rot_deg != 180 && rot_deg != 270) rot_deg = 0;
    lv_display_t *disp = lv_display_create(w, h);
    lv_display_set_driver_data(disp, drv);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);

    bool need_rotate = (rot_deg != 0);
    if (need_rotate) drv->direct = false;  /* rotation requires DMA FULL */

    /* ---- Buffer setup: single allocation ---- */
    if (!drv->draw_buf) {
        if (drv->direct) {
            /* Path B: DIRECT mode (zero-copy, no rotation) */
            drv->draw_buf = xosfb_get_fb_ptr(drv->ctx);
            lv_display_set_buffers(disp, drv->draw_buf, NULL,
                                   drv->fb_stride * h,
                                   LV_DISPLAY_RENDER_MODE_DIRECT);
            LV_LOG_INFO("XOSFB-V2: DIRECT mode");
        } else if (drv->use_vgs2) {
            uint32_t buf_bytes = w * h * 4;
            if (need_rotate) {
                /* Rotation: LVGL renders to CACHED buffer (fast).
                 * Flush: memcpy dirty → DMA → VGS2 rotate → FB. */
                drv->draw_buf = lv_malloc(buf_bytes);
                LV_ASSERT_MALLOC(drv->draw_buf);
                if (xosfb_v2_alloc_dma(drv->ctx, buf_bytes, &drv->dma) != 0)
                    drv->use_vgs2 = false;
                lv_display_set_buffers(disp, drv->draw_buf, NULL, buf_bytes,
                                       LV_DISPLAY_RENDER_MODE_FULL);
                LV_LOG_INFO("XOSFB-V2: FULL + cached + VGS2 rotate");
            } else {
                /* No rotation: DMA buffer for both LVGL + VGS2 source */
                if (xosfb_v2_alloc_dma(drv->ctx, buf_bytes, &drv->dma) == 0) {
                    drv->draw_buf = drv->dma.virt_addr;
                    lv_display_set_buffers(disp, drv->draw_buf, NULL, buf_bytes,
                                           LV_DISPLAY_RENDER_MODE_FULL);
                    LV_LOG_INFO("XOSFB-V2: FULL + VGS blit");
                } else {
                    drv->use_vgs2 = false;
                }
            }
        }
        if (!drv->draw_buf) {
            /* Path D: CPU fallback */
            uint32_t buf_bytes = w * h * drv->px_size;
            drv->draw_buf = lv_malloc(buf_bytes);
            LV_ASSERT_MALLOC(drv->draw_buf);
            lv_display_set_buffers(disp, drv->draw_buf, NULL, buf_bytes,
                                   LV_DISPLAY_RENDER_MODE_FULL);
            LV_LOG_INFO("XOSFB-V2: FULL + CPU fallback");
        }
    }

    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_add_event_cb(disp, del_cb, LV_EVENT_DELETE, NULL);
    lv_display_add_event_cb(disp, render_start_cb, LV_EVENT_RENDER_START, NULL);

    /* Rotation: VGS2 hardware partial rotate (per dirty area). */
    if (need_rotate && drv->use_vgs2 && (caps & XOSFB_V2_CAP_ROTATE) && drv->dma.virt_addr) {
        drv->hw_rotate = true;
        drv->hw_deg    = rot_deg;
        lv_display_rotation_t lr = LV_DISPLAY_ROTATION_0;
        if (rot_deg == 90)  lr = LV_DISPLAY_ROTATION_90;
        if (rot_deg == 180) lr = LV_DISPLAY_ROTATION_180;
        if (rot_deg == 270) lr = LV_DISPLAY_ROTATION_270;
        lv_display_set_rotation(disp, lr);
        LV_LOG_USER("XOSFB-V2: rotation %d° (VGS2 partial rotate)", rot_deg);
    } else if (need_rotate) {
        LV_LOG_USER("XOSFB-V2: rotation %d° unsupported (need VGS2+DMA)", rot_deg);
    } else {
        LV_LOG_USER("XOSFB-V2: rotation NONE");
    }

    drv->dbg.level = debug_parse_level();
    if (drv->dbg.level)
        LV_LOG_USER("XOSFB-V2: debug level %d (1=summary 2=per-frame)", drv->dbg.level);

    LV_LOG_INFO("XOSFB-V2 ready");
    return disp;
}

static void render_start_cb(lv_event_t *e)
{
    lv_display_t *disp = lv_event_get_target(e);
    xosfb_v2_drv_t *drv = lv_display_get_driver_data(disp);
    if (drv && drv->dbg.level)
        clock_gettime(CLOCK_MONOTONIC, &drv->dbg.frame_start);
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    xosfb_v2_drv_t *drv = lv_display_get_driver_data(disp);
    if (!drv || !drv->ctx) { lv_display_flush_ready(disp); return; }

    int rw = lv_area_get_width(area), rh = lv_area_get_height(area);
    if (rw <= 0 || rh <= 0) { lv_display_flush_ready(disp); return; }

    if (drv->hw_rotate) {
        int stride = drv->w * drv->px_size;
        int line_bytes = rw * drv->px_size;
        struct timespec t0, t1;
        int lv = drv->dbg.level;

        /* Phase 1: copy dirty area cached draw_buf → DMA buffer */
        if (lv) clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int y = 0; y < rh; y++) {
            uint8_t *s = drv->draw_buf + (area->y1 + y)*stride + area->x1*drv->px_size;
            uint8_t *d = (uint8_t*)drv->dma.virt_addr
                       + (area->y1 + y)*stride + area->x1*drv->px_size;
            memcpy(d, s, line_bytes);
        }
        if (lv) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
            drv->dbg.memcpy_us += debug_elapsed(&t0, &t1);
        }

        /* Phase 2: VGS2 rotate dirty area DMA → FB */
        xosfb_v2_dma_buf_t src = drv->dma;
        src.phy_addr += (area->y1 * drv->w + area->x1) * drv->px_size;
        src.virt_addr = (uint8_t*)drv->dma.virt_addr
                      + (area->y1 * drv->w + area->x1) * drv->px_size;
        int dst_x, dst_y;
        if (drv->hw_deg == 180) {
            dst_x = drv->w - area->x1 - rw;
            dst_y = drv->h - area->y1 - rh;
        } else if (drv->hw_deg == 90) {
            dst_x = drv->h - area->y1 - rh;
            dst_y = area->x1;
        } else {
            dst_x = area->y1;
            dst_y = drv->w - area->x1 - rw;
        }
        xosfb_v2_rotation_t rot = XOSFB_V2_ROTATE_0;
        if (drv->hw_deg == 90)       rot = XOSFB_V2_ROTATE_90;
        else if (drv->hw_deg == 180) rot = XOSFB_V2_ROTATE_180;
        else if (drv->hw_deg == 270) rot = XOSFB_V2_ROTATE_270;

        if (lv) clock_gettime(CLOCK_MONOTONIC, &t0);
        xosfb_v2_rotate_blit(drv->ctx, &src, rw, rh,
                             XOSFB_FMT_ARGB8888, dst_x, dst_y, rot);
        if (lv) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
            drv->dbg.vgs2_us += debug_elapsed(&t0, &t1);
        }

        debug_flush_add(drv, rw, rh);

        /* Phase 3: pan on last flush */
        if (lv_display_flush_is_last(disp)) {
            if (lv) clock_gettime(CLOCK_MONOTONIC, &t0);
            xosfb_pan_display(drv->ctx);
            if (lv) {
                clock_gettime(CLOCK_MONOTONIC, &t1);
                drv->dbg.pan_us += debug_elapsed(&t0, &t1);
            }
            debug_frame_end(drv);
            debug_summary_try(drv);
        }
        lv_display_flush_ready(disp);
        return;
    }
    if (drv->direct) {
    } else if (drv->use_vgs2) {
        uint8_t *fb = xosfb_get_fb_ptr(drv->ctx);
        int fb_stride = drv->fb_stride;
        int src_stride = drv->w * 4;
        if (fb && fb_stride == src_stride && area->x1 == 0)
            memcpy(fb, &px[area->y1 * src_stride], rh * src_stride);
        else if (fb)
            for (int y = 0; y < rh; y++)
                memcpy(&fb[(area->y1+y)*fb_stride + area->x1*drv->px_size],
                       &px[y*src_stride], rw * drv->px_size);
    } else {
        uint8_t *fb = xosfb_get_fb_ptr(drv->ctx);
        int fb_stride = drv->fb_stride;
        int src_stride = drv->w * drv->px_size;
        if (!fb) { lv_display_flush_ready(disp); return; }

        if (!lv_display_flush_is_last(disp)) {
            for (int y = 0; y < rh; y++)
                memcpy(fb + (area->y1 + y) * fb_stride + area->x1 * drv->px_size,
                       px + y * src_stride, rw * drv->px_size);
            lv_display_flush_ready(disp);
            return;
        }
        for (int y = 0; y < rh; y++)
            memcpy(fb + (area->y1 + y) * fb_stride + area->x1 * drv->px_size,
                   px + y * src_stride, rw * drv->px_size);
    }

    if (lv_display_flush_is_last(disp))
        xosfb_pan_display(drv->ctx);
    lv_display_flush_ready(disp);
}

static void del_cb(lv_event_t *e)
{
    if (LV_EVENT_DELETE != lv_event_get_code(e)) return;
    lv_display_t *disp = lv_event_get_target(e);
    xosfb_v2_drv_t *drv = lv_display_get_driver_data(disp);
    if (!drv) return;

    /* Only free what we allocated; FB pointer is owned by xosfb */
    if (drv->use_vgs2 && drv->dma.virt_addr)
        xosfb_v2_free_dma(drv->ctx, &drv->dma);
    if (!drv->direct && drv->draw_buf && drv->draw_buf != drv->dma.virt_addr)
        lv_free(drv->draw_buf);
    if (drv->ctx) xosfb_v2_exit(drv->ctx);
    lv_free(drv);
}

#endif /* LV_USE_LINUX_FBDEV */
