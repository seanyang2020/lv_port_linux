/**
 * @file xosfb_v2_lvgl.c — LVGL display driver for xosfb v2
 *
 * Minimal glue between LVGL and libxosfb_v2.a.
 * Based on §6 of xosfb_v2.md (2026-06-15, standalone edition).
 *
 * Required:  xosfb_v2.h + libxosfb_v2.a (self-contained, includes libmpi.a)
 * NOT needed: xosfb.h / libxosfb.a (v1 files)
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

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    xosfb_ctx_t       *ctx;
    xosfb_v2_dma_buf_t dma;
    uint8_t           *draw_buf;   /* DMA virt_addr or malloc fallback */
    int                px_size;
    int                w, h;
    bool               use_vgs2;   /* VGS2 hardware blit available */
} xosfb_v2_drv_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_display_t *init_xosfb_v2(void);
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px);
static void del_cb(lv_event_t *e);
static uint32_t tick_get_cb(void);

/**********************
 *   STATIC VARIABLES
 **********************/
static char *backend_name = "XOSFB-V2";

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int backend_init_xosfb(backend_t *b)
{
    b->handle->display = malloc(sizeof(display_backend_t));
    LV_ASSERT_NULL(b->handle->display);
    b->handle->display->init_display = init_xosfb_v2;
    b->handle->display->run_loop     = NULL;
    b->name = backend_name;
    b->type = BACKEND_DISPLAY;
    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
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

    /* ---- Init xosfb v2 (FB=16bpp saves DDR bandwidth) ---- */
    drv->ctx = xosfb_v2_init(w, h, XOSFB_FMT_ARGB1555);
    if (!drv->ctx) { LV_LOG_ERROR("xosfb_v2_init failed"); lv_free(drv); return NULL; }
    drv->w = w; drv->h = h;
    drv->px_size = xosfb_get_bpp(drv->ctx) / 8;

    /* Check caps: VGS2 needed for CSC blit (LVGL 32bpp→FB 16bpp) */
    uint32_t caps = xosfb_v2_get_caps(drv->ctx);
    drv->use_vgs2 = (caps & XOSFB_V2_CAP_CONVERT) != 0;
    LV_LOG_INFO("XOSFB-V2: %dx%d bpp=%d caps=0x%x VGS2=%d",
                w, h, drv->px_size * 8, caps, drv->use_vgs2);

    /* ---- LVGL draw buffer (32bpp ARGB8888, DMA preferred) ---- */
    uint32_t buf_bytes = w * h * 4;  /* LVGL always 32bpp */
    if (drv->use_vgs2 && xosfb_v2_alloc_dma(drv->ctx, buf_bytes, &drv->dma) == 0) {
        drv->draw_buf = drv->dma.virt_addr;
        LV_LOG_INFO("LVGL DMA buffer: virt=%p phy=0x%lx", drv->dma.virt_addr, drv->dma.phy_addr);
    } else {
        drv->draw_buf = lv_malloc(buf_bytes);
        LV_ASSERT_MALLOC(drv->draw_buf);
        drv->use_vgs2 = false; /* no DMA → no VGS2 */
        LV_LOG_WARN("DMA unavailable, CPU memcpy fallback");
    }

    /* ---- LVGL display ---- */
    lv_display_t *disp = lv_display_create(w, h);
    lv_display_set_driver_data(disp, drv);
    /* LVGL renders 32bpp; VGS2 converts to 16bpp FB, or CPU fallback */
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(disp, drv->draw_buf, NULL, buf_bytes,
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_add_event_cb(disp, del_cb, LV_EVENT_DELETE, NULL);

    LV_LOG_INFO("XOSFB-V2 ready");
    return disp;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    xosfb_v2_drv_t *drv = lv_display_get_driver_data(disp);
    if (!drv || !drv->ctx) { lv_display_flush_ready(disp); return; }

    int w = lv_area_get_width(area), h = lv_area_get_height(area);
    if (w <= 0 || h <= 0) { lv_display_flush_ready(disp); return; }

    if (drv->use_vgs2) {
        /* VGS2 hardware blit: LVGL 32bpp → FB 16bpp (~2ms) */
        xosfb_v2_blit_desc_t d = {
            .src_buf = px, .src_w = w, .src_h = h,
            .src_stride = drv->w, .src_fmt = XOSFB_FMT_ARGB8888,
            .dst_x = area->x1, .dst_y = area->y1, .dst_w = w, .dst_h = h,
        };
        xosfb_v2_blit(drv->ctx, &d);
    } else {
        /* CPU fallback: 32bpp ARGB → 16bpp native */
        uint8_t *fb = xosfb_get_fb_ptr(drv->ctx);
        int fb_stride = xosfb_get_line_length(drv->ctx);
        if (fb) {
            for (int y = 0; y < h; y++) {
                uint32_t *src = (uint32_t *)(px + y * drv->w * 4);
                uint16_t *dst = (uint16_t *)(fb + (area->y1 + y) * fb_stride
                                             + area->x1 * 2);
                for (int x = 0; x < w; x++)
                    dst[x] = (uint16_t)((src[x] >> 19) << 10)  /* R */
                           | (uint16_t)((src[x] >> 10) & 0x1F); /* G+B approx */
            }
        }
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

    if (drv->dma.virt_addr) xosfb_v2_free_dma(drv->ctx, &drv->dma);
    else if (drv->draw_buf) lv_free(drv->draw_buf);
    if (drv->ctx) xosfb_v2_exit(drv->ctx);
    lv_free(drv);
}

#endif /* LV_USE_LINUX_FBDEV */
