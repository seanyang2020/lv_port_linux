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
    xosfb_v2_dma_buf_t dma;
    uint8_t           *draw_buf;
    int                px_size;
    int                fb_stride;   /* actual FB line stride (bytes) */
    int                w, h;
    bool               use_vgs2;
    bool               direct;      /* DIRECT mode (no flush copy needed) */
} xosfb_v2_drv_t;

static lv_display_t *init_xosfb_v2(void);
static void run_loop_xosfb_v2(void);
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px);
static void del_cb(lv_event_t *e);
static uint32_t tick_get_cb(void);

static char *backend_name = "XOSFB-V2";

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

    /* ---- Buffer setup ---- */
    lv_display_t *disp = lv_display_create(w, h);
    lv_display_set_driver_data(disp, drv);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);

    if (drv->direct) {
        /*
         * DIRECT mode: LVGL renders straight to FB memory.
         * Zero memcpy — flush only calls pan_display.
         * TDE2 fill_rect can be used for background clears.
         */
        drv->draw_buf = xosfb_get_fb_ptr(drv->ctx);
        uint32_t fb_bytes = drv->fb_stride * h; /* use actual FB stride * height */
        lv_display_set_buffers(disp, drv->draw_buf, NULL, fb_bytes,
                               LV_DISPLAY_RENDER_MODE_DIRECT);
        LV_LOG_INFO("XOSFB-V2: DIRECT mode (zero-copy), fb=%p stride=%d",
                    drv->draw_buf, drv->fb_stride);
    } else if (drv->use_vgs2) {
        /*
         * FULL mode + DMA buffer: VGS2 blits LVGL buffer → FB.
         * DMA required for VGS2 hardware access.
         */
        uint32_t buf_bytes = w * h * 4;
        if (xosfb_v2_alloc_dma(drv->ctx, buf_bytes, &drv->dma) == 0) {
            drv->draw_buf = drv->dma.virt_addr;
            lv_display_set_buffers(disp, drv->draw_buf, NULL, buf_bytes,
                                   LV_DISPLAY_RENDER_MODE_FULL);
            LV_LOG_INFO("XOSFB-V2: FULL + VGS2 mode, dma=%p", drv->draw_buf);
        } else {
            drv->use_vgs2 = false; /* fall through */
        }
    }

    if (!drv->direct && !drv->use_vgs2) {
        /*
         * Fallback: FULL mode CPU memcpy (safe, works everywhere).
         * Neither DIRECT (stride mismatch) nor VGS2 (no driver) available.
         */
        uint32_t buf_bytes = w * h * drv->px_size;
        drv->draw_buf = lv_malloc(buf_bytes);
        LV_ASSERT_MALLOC(drv->draw_buf);
        lv_display_set_buffers(disp, drv->draw_buf, NULL, buf_bytes,
                               LV_DISPLAY_RENDER_MODE_FULL);
        LV_LOG_INFO("XOSFB-V2: FULL + CPU fallback, buf=%p", drv->draw_buf);
    }

    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_add_event_cb(disp, del_cb, LV_EVENT_DELETE, NULL);

    LV_LOG_INFO("XOSFB-V2 ready");
    return disp;
}

static void run_loop_xosfb_v2(void)
{
    while (true) {
        uint32_t idle_time = lv_timer_handler();
        usleep(idle_time * 1000);
    }
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    xosfb_v2_drv_t *drv = lv_display_get_driver_data(disp);
    if (!drv || !drv->ctx) { lv_display_flush_ready(disp); return; }

    int w = lv_area_get_width(area), h = lv_area_get_height(area);
    if (w <= 0 || h <= 0) { lv_display_flush_ready(disp); return; }

    if (drv->direct) {
        /* DIRECT mode: LVGL already wrote to FB. No copy needed.
         * Optionally use TDE2 fill for full-screen background on first frame. */
    } else if (drv->use_vgs2) {
        /* VGS2 hardware blit: DMA buffer → FB */
        xosfb_v2_blit_desc_t d = {
            .src_buf = px, .src_w = w, .src_h = h,
            .src_stride = drv->w, .src_fmt = XOSFB_FMT_ARGB8888,
            .dst_x = area->x1, .dst_y = area->y1, .dst_w = w, .dst_h = h,
        };
        xosfb_v2_blit(drv->ctx, &d);
    } else {
        /* FULL + CPU fallback: memcpy LVGL buffer → FB (same format, no conversion) */
        uint8_t *fb = xosfb_get_fb_ptr(drv->ctx);
        int fb_stride = drv->fb_stride;
        int src_stride = drv->w * drv->px_size;
        if (!fb) { lv_display_flush_ready(disp); return; }

        /* Partial flush: only copy dirty areas */
        if (!lv_display_flush_is_last(disp)) {
            for (int y = 0; y < h; y++)
                memcpy(fb + (area->y1 + y) * fb_stride + area->x1 * drv->px_size,
                       px + y * src_stride, w * drv->px_size);
            lv_display_flush_ready(disp);
            return;
        }
        /* Last flush: copy final area + pan */
        for (int y = 0; y < h; y++)
            memcpy(fb + (area->y1 + y) * fb_stride + area->x1 * drv->px_size,
                   px + y * src_stride, w * drv->px_size);
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
    else if (!drv->direct && drv->draw_buf)
        lv_free(drv->draw_buf);
    if (drv->ctx) xosfb_v2_exit(drv->ctx);
    lv_free(drv);
}

#endif /* LV_USE_LINUX_FBDEV */
