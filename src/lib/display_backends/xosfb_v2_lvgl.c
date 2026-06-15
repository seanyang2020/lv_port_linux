/**
 * @file xosfb_v2_lvgl.c — LVGL display driver for xosfb v2
 *
 * Minimal glue between LVGL and libxosfb_v2.a (TDE2+VGS2 hardware).
 * Based on §6 of xosfb_v2.md integration template.
 *
 * Required: libxosfb_v2.a + xosfb_v2.h + xosfb.h
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

/* ---- Stub: SDK xosfb.o references SAMPLE_COMM_VO_StartDev which our
 *     init path (xosfb_v2_init) does not use. Satisfy the linker. ---- */
extern int SAMPLE_COMM_VO_StartDev(int, int, int, int, int, int);
int SAMPLE_COMM_VO_StartDev(int a, int b, int c, int d, int e, int f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 0;
}

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    xosfb_ctx_t       *ctx;
    xosfb_v2_dma_buf_t dma;
    uint8_t           *draw_buf;
    int                px_size;
    int                w, h;
    bool               use_dma;
} xosfb_v2_drv_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_display_t *init_xosfb_v2(void);
static void run_loop_xosfb_v2(void);
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *color_p);
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
    b->handle->display->run_loop     = run_loop_xosfb_v2;
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

static void run_loop_xosfb_v2(void)
{
    /* Main loop handled by driver_backends PAN polling */
}

static lv_display_t *init_xosfb_v2(void)
{
    /* Read env */
    const char *ew = getenv("LV_XOSFB_WIDTH"), *eh = getenv("LV_XOSFB_HEIGHT");
    int w = ew ? atoi(ew) : 800, h = eh ? atoi(eh) : 1280;

    lv_tick_set_cb(tick_get_cb);

    xosfb_v2_drv_t *drv = lv_malloc_zeroed(sizeof(xosfb_v2_drv_t));
    LV_ASSERT_MALLOC(drv);

    /* ---- Init xosfb v2 ---- */
    drv->ctx = xosfb_v2_init(w, h, XOSFB_FMT_ARGB1555);
    if (!drv->ctx) { LV_LOG_ERROR("xosfb_v2_init failed"); lv_free(drv); return NULL; }

    /* Detect pixel format */
    drv->w = w; drv->h = h;
    switch (xosfb_get_pixel_format(drv->ctx)) {
        case XOSFB_FMT_ARGB8888: drv->px_size = 4; break;
        default:                  drv->px_size = 2; break;
    }

    /* ---- Allocate LVGL draw buffer (DMA preferred) ---- */
    uint32_t buf_size = w * h * drv->px_size;
    if (xosfb_v2_alloc_dma(drv->ctx, buf_size, &drv->dma) == 0) {
        drv->draw_buf = drv->dma.virt_addr;
        drv->use_dma  = true;
        LV_LOG_INFO("XOSFB-V2 DMA buffer: virt=%p phy=0x%lx", drv->dma.virt_addr, drv->dma.phy_addr);
    } else {
        drv->draw_buf = lv_malloc(buf_size);
        LV_ASSERT_MALLOC(drv->draw_buf);
        drv->use_dma = false;
        LV_LOG_WARN("DMA alloc failed, using malloc (no VGS2 blit)");
    }

    /* ---- LVGL display ---- */
    lv_display_t *disp = lv_display_create(w, h);
    lv_display_set_driver_data(disp, drv);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565); /* 16bpp matches ARGB1555 */
    lv_display_set_buffers(disp, drv->draw_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_add_event_cb(disp, del_cb, LV_EVENT_DELETE, NULL);

    LV_LOG_INFO("XOSFB-V2 ready: %dx%d", w, h);
    return disp;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    xosfb_v2_drv_t *drv = lv_display_get_driver_data(disp);
    if (!drv || !drv->ctx) { lv_display_flush_ready(disp); return; }
    if (!lv_display_flush_is_last(disp)) { lv_display_flush_ready(disp); return; }

    int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);

    if (drv->use_dma) {
        /* ---- VGS2 hardware blit with CSC (~2ms) ---- */
        xosfb_v2_blit_desc_t d = {
            .src_buf = color_p, .src_w = w, .src_h = h,
            .src_stride = drv->w, .src_fmt = XOSFB_FMT_ARGB8888,
            .dst_x = area->x1, .dst_y = area->y1, .dst_w = w, .dst_h = h,
        };
        xosfb_v2_blit(drv->ctx, &d);
    } else {
        /* ---- CPU fallback ---- */
        uint8_t *fb = xosfb_get_fb_ptr(drv->ctx);
        int fbs = xosfb_get_line_length(drv->ctx);
        int src_s = drv->w * drv->px_size;
        if (fb) {
            if (fbs == src_s && area->x1 == 0)
                lv_memcpy(fb, &color_p[area->y1 * src_s], h * src_s);
            else
                for (int32_t y = 0; y < h; y++)
                    lv_memcpy(&fb[(area->y1+y)*fbs + area->x1*drv->px_size],
                              &color_p[y * src_s], w * drv->px_size);
        }
    }

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
