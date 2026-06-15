/**
 * @file xosfb_v2_lvgl.c — LVGL display driver for xosfb v2
 *
 * Minimal glue between LVGL and libxosfb_v2.a.
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
    uint8_t           *draw_buf;
    int                px_size;
    int                w, h;
    bool               use_vgs2;
} xosfb_v2_drv_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_display_t *init_xosfb_v2(void);
static void run_loop_xosfb_v2(void);
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

static lv_display_t *init_xosfb_v2(void)
{
    const char *ew = getenv("LV_XOSFB_WIDTH"), *eh = getenv("LV_XOSFB_HEIGHT");
    int w = ew ? atoi(ew) : 800, h = eh ? atoi(eh) : 1280;

    lv_tick_set_cb(tick_get_cb);

    xosfb_v2_drv_t *drv = lv_malloc_zeroed(sizeof(xosfb_v2_drv_t));
    LV_ASSERT_MALLOC(drv);
    if (!drv) return NULL;

    /* FB & LVGL both ARGB8888 — no format conversion needed */
    drv->ctx = xosfb_v2_init(w, h, XOSFB_FMT_ARGB8888);
    if (!drv->ctx) { LV_LOG_ERROR("xosfb_v2_init failed"); lv_free(drv); return NULL; }
    drv->w = w; drv->h = h;
    drv->px_size = xosfb_get_bpp(drv->ctx) / 8;

    uint32_t caps = xosfb_v2_get_caps(drv->ctx);
    drv->use_vgs2 = (caps & XOSFB_V2_CAP_CONVERT) != 0;
    LV_LOG_INFO("XOSFB-V2: %dx%d bpp=%d caps=0x%x VGS2=%d",
                w, h, drv->px_size * 8, caps, drv->use_vgs2);

    /* Single full-screen buffer: qm10xd requires FULL mode */
    uint32_t buf_size = w * h * drv->px_size;
    drv->draw_buf = lv_malloc(buf_size);
    LV_ASSERT_MALLOC(drv->draw_buf);

    lv_display_t *disp = lv_display_create(w, h);
    lv_display_set_driver_data(disp, drv);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(disp, drv->draw_buf, NULL, buf_size,
                           LV_DISPLAY_RENDER_MODE_FULL);
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

    /* FULL mode: only last flush carries complete frame */
    if (!lv_display_flush_is_last(disp)) { lv_display_flush_ready(disp); return; }

    int w = lv_area_get_width(area), h = lv_area_get_height(area);
    if (w <= 0 || h <= 0) { lv_display_flush_ready(disp); return; }

    uint8_t *fb = xosfb_get_fb_ptr(drv->ctx);
    int fb_stride = xosfb_get_line_length(drv->ctx);
    int src_stride = drv->w * drv->px_size;
    if (!fb) { lv_display_flush_ready(disp); return; }

    if (fb_stride == src_stride && area->x1 == 0)
        memcpy(fb, &px[area->y1 * src_stride], h * src_stride);
    else
        for (int y = 0; y < h; y++)
            memcpy(&fb[(area->y1 + y) * fb_stride + area->x1 * drv->px_size],
                   &px[(area->y1 + y) * src_stride + area->x1 * drv->px_size],
                   w * drv->px_size);

    xosfb_pan_display(drv->ctx);
    lv_display_flush_ready(disp);
}

static void del_cb(lv_event_t *e)
{
    if (LV_EVENT_DELETE != lv_event_get_code(e)) return;
    lv_display_t *disp = lv_event_get_target(e);
    xosfb_v2_drv_t *drv = lv_display_get_driver_data(disp);
    if (!drv) return;

    if (drv->draw_buf) lv_free(drv->draw_buf);
    if (drv->ctx) xosfb_v2_exit(drv->ctx);
    lv_free(drv);
}

#endif /* LV_USE_LINUX_FBDEV */
