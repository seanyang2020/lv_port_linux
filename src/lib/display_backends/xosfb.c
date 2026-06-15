/**
 * @file xosfb.c
 *
 * XOS Framebuffer display backend for LVGL
 *
 * Uses the xosfb-lib.a library to drive the qm10xd hardware framebuffer.
 * Replaces the standard Linux fbdev backend with platform-optimized
 * initialization and FBIOPAN_DISPLAY-based buffer flipping.
 *
 * Usage:
 *   Set LV_XOSFB_FORMAT env var: ARGB8888 (default), ARGB1555, ARGB0565
 *
 * Based on fbdev.c backend pattern.
 * 2025 EDGEMTech Ltd.
 */

/*********************
 *      INCLUDES
 *********************/
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "lvgl/lvgl.h"
#include "../simulator_util.h"
#include "../backends.h"
#include "xosfb.h"
#include "xosfb_v2.h"

#if LV_USE_LINUX_FBDEV

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    xosfb_ctx_t *xosfb;           /* xosfb library context */
    uint8_t     *draw_buf_1;      /* LVGL draw buffer 1 (DMA or malloc) */
    uint8_t     *draw_buf_2;      /* LVGL draw buffer 2 (optional) */
    xosfb_v2_dma_buf_t dma_buf;   /* DMA buffer handle (v2 only) */
    bool         use_v2;          /* true if v2 hardware available */
    int          px_size;         /* bytes per pixel */
    lv_color_format_t lvgl_fmt;   /* LVGL color format */
    bool         needs_alpha_convert; /* true if ARGB1555 needs bit15 set */
} xosfb_drv_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static lv_display_t *init_xosfb(void);
static void run_loop_xosfb(void);
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p);
static void del_event_cb(lv_event_t *e);
static uint32_t tick_get_cb(void);

/**********************
 *  STATIC VARIABLES
 **********************/

static char *backend_name = "XOSFB";

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Register the XOSFB backend
 * @param backend the backend descriptor
 * @return 0 on success
 */
int backend_init_xosfb(backend_t *backend)
{
    LV_ASSERT_NULL(backend);

    backend->handle->display = malloc(sizeof(display_backend_t));
    LV_ASSERT_NULL(backend->handle->display);

    backend->handle->display->init_display = init_xosfb;
    backend->handle->display->run_loop = run_loop_xosfb;
    backend->name = backend_name;
    backend->type = BACKEND_DISPLAY;

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Parse pixel format from environment variable
 * @return xosfb_pixel_format_t value, defaults to ARGB8888
 */
static xosfb_pixel_format_t parse_format_env(void)
{
    const char *env = getenv("XOSFB_FORMAT");
    if (!env) return XOSFB_FMT_ARGB8888;

    if (strcmp(env, "ARGB1555") == 0) return XOSFB_FMT_ARGB1555;
    if (strcmp(env, "ARGB0565") == 0) return XOSFB_FMT_ARGB0565;
    if (strcmp(env, "ARGB8888") == 0) return XOSFB_FMT_ARGB8888;

    LV_LOG_WARN("Unknown XOSFB_FORMAT '%s', defaulting to ARGB8888", env);
    return XOSFB_FMT_ARGB8888;
}

/**
 * Initialize the XOSFB display backend
 * @return the LVGL display or NULL on error
 */
static lv_display_t *init_xosfb(void)
{
    /* Read environment for resolution and format */
    const char *env_w = getenv("LV_XOSFB_WIDTH");
    const char *env_h = getenv("LV_XOSFB_HEIGHT");
    int width = env_w ? atoi(env_w) : 800;
    int height = env_h ? atoi(env_h) : 1280;
    xosfb_pixel_format_t fmt = parse_format_env();

    LV_LOG_INFO("Initializing XOSFB: %dx%d fmt=%d", width, height, fmt);

    /* Set tick callback before any LVGL timer usage */
    lv_tick_set_cb(tick_get_cb);

    /* Allocate driver data */
    xosfb_drv_t *drv = lv_malloc_zeroed(sizeof(xosfb_drv_t));
    LV_ASSERT_MALLOC(drv);
    if (!drv) return NULL;

    /* Initialize xosfb library (v2 hardware accelerated if available) */
    drv->xosfb = xosfb_v2_init(width, height, fmt);
    drv->use_v2 = (drv->xosfb != NULL);
    if (!drv->xosfb) {
        LV_LOG_ERROR("xosfb_v2_init failed");
        lv_free(drv);
        return NULL;
    }
    LV_LOG_INFO("xosfb v%d mode: %s", drv->use_v2 ? 2 : 1,
                drv->use_v2 ? "TDE2+VGS2 accelerated" : "CPU memcpy");

    /* Get actual resolution from driver */
    int actual_w, actual_h;
    xosfb_get_resolution(drv->xosfb, &actual_w, &actual_h);
    drv->px_size = xosfb_get_bpp(drv->xosfb) / 8;

    /* Map xosfb format to LVGL color format */
    switch (xosfb_get_pixel_format(drv->xosfb)) {
        case XOSFB_FMT_ARGB8888:
            drv->lvgl_fmt = LV_COLOR_FORMAT_ARGB8888;
            drv->needs_alpha_convert = false;
            LV_LOG_INFO("XOSFB format: ARGB8888");
            break;
        case XOSFB_FMT_ARGB0565:
            drv->lvgl_fmt = LV_COLOR_FORMAT_RGB565;
            drv->needs_alpha_convert = false;
            LV_LOG_INFO("XOSFB format: ARGB0565 (RGB565)");
            break;
        case XOSFB_FMT_ARGB1555:
            drv->lvgl_fmt = LV_COLOR_FORMAT_RGB565;
            drv->needs_alpha_convert = true;
            LV_LOG_INFO("XOSFB format: ARGB1555 (alpha-bit conversion enabled)");
            break;
        default:
            drv->lvgl_fmt = LV_COLOR_FORMAT_ARGB8888;
            drv->needs_alpha_convert = false;
            break;
    }

    /* Create LVGL display */
    lv_display_t *disp = lv_display_create(actual_w, actual_h);
    if (!disp) {
        LV_LOG_ERROR("lv_display_create failed");
        if (drv->dma_buf.virt_addr) xosfb_v2_free_dma(drv->xosfb, &drv->dma_buf);
        xosfb_v2_exit(drv->xosfb);
        lv_free(drv);
        return NULL;
    }

    lv_display_set_driver_data(disp, drv);
    lv_display_set_color_format(disp, drv->lvgl_fmt);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_add_event_cb(disp, del_event_cb, LV_EVENT_DELETE, NULL);

    /* Allocate draw buffer — DMA (v2) for VGS2 hardware blit, malloc fallback */
    uint32_t draw_buf_size = actual_w * actual_h * drv->px_size;
    if (drv->use_v2 && xosfb_v2_alloc_dma(drv->xosfb, draw_buf_size, &drv->dma_buf) == 0) {
        drv->draw_buf_1 = drv->dma_buf.virt_addr;
        LV_LOG_INFO("LVGL DMA buffer: virt=%p phy=0x%lx size=%u",
                    drv->dma_buf.virt_addr, drv->dma_buf.phy_addr, draw_buf_size);
    } else {
        drv->draw_buf_1 = lv_malloc(draw_buf_size);
        LV_ASSERT_MALLOC(drv->draw_buf_1);
        drv->use_v2 = false; /* can't use VGS2 blit without DMA */
    }
    drv->draw_buf_2 = NULL;

    lv_display_set_buffers(disp, drv->draw_buf_1, drv->draw_buf_2,
                           draw_buf_size, LV_DISPLAY_RENDER_MODE_FULL);

    LV_LOG_INFO("XOSFB display ready: %dx%d, draw_buf=%u bytes",
                actual_w, actual_h, draw_buf_size);

    return disp;
}

/**
 * LVGL flush callback — FULL render mode.
 * Skip non-last flushes; on last flush, copy entire area to mmap'd
 * framebuffer and call FBIOPAN_DISPLAY to commit.  Matches the pattern
 * in fb_test.c: draw entire screen → pan.
 */
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    xosfb_drv_t *drv = lv_display_get_driver_data(disp);
    if (!drv || !drv->xosfb) {
        lv_display_flush_ready(disp);
        return;
    }

    /* FULL mode: only the last flush carries the complete frame */
    if (!lv_display_flush_is_last(disp)) {
        lv_display_flush_ready(disp);
        return;
    }

    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    int hor_res = lv_display_get_horizontal_resolution(disp);

    if (drv->use_v2 && drv->dma_buf.virt_addr) {
        /* VGS2 hardware blit (CSC + DMA, ~2ms) */
        xosfb_v2_blit_desc_t desc = {
            .src_buf = color_p, .src_w = w, .src_h = h,
            .src_stride = hor_res, .src_fmt = XOSFB_FMT_ARGB8888,
            .dst_x = area->x1, .dst_y = area->y1, .dst_w = w, .dst_h = h,
        };
        xosfb_v2_blit(drv->xosfb, &desc);
    } else {
        /* CPU memcpy fallback */
        uint8_t *fb = xosfb_get_fb_ptr(drv->xosfb);
        int fb_stride = xosfb_get_line_length(drv->xosfb);
        int src_stride = hor_res * drv->px_size;
        if (!fb) { lv_display_flush_ready(disp); return; }
        if (fb_stride == src_stride && area->x1 == 0)
            lv_memcpy(fb, &color_p[area->y1 * src_stride], h * src_stride);
        else
            for (int32_t y = 0; y < h; y++) {
                uint32_t fo = (area->y1 + y) * fb_stride + area->x1 * drv->px_size;
                uint32_t so = (area->y1 + y) * src_stride + area->x1 * drv->px_size;
                lv_memcpy(&fb[fo], &color_p[so], w * drv->px_size);
            }
    }

    /* Commit to display — required for qm10xd hardware */
    xosfb_pan_display(drv->xosfb);

    lv_display_flush_ready(disp);
}

/**
 * Cleanup callback when LVGL display is deleted
 */
static void del_event_cb(lv_event_t *e)
{
    if (LV_EVENT_DELETE != lv_event_get_code(e))
        return;

    lv_display_t *disp = lv_event_get_target(e);
    xosfb_drv_t *drv = lv_display_get_driver_data(disp);
    if (!drv) return;

    if (drv->xosfb) {
        if (drv->dma_buf.virt_addr) xosfb_v2_free_dma(drv->xosfb, &drv->dma_buf);
        xosfb_v2_exit(drv->xosfb);
        drv->xosfb = NULL;
    }
    /* DMA buffer freed above, malloc buffer freed here */
    if (!drv->dma_buf.virt_addr && drv->draw_buf_1) lv_free(drv->draw_buf_1);
    if (drv->draw_buf_2) lv_free(drv->draw_buf_2);

    lv_free(drv);
    lv_display_set_driver_data(disp, NULL);
}

/**
 * The run loop — calls lv_timer_handler() in a loop
 */
static void run_loop_xosfb(void)
{
    while (true) {
        uint32_t idle_time = lv_timer_handler();
        usleep(idle_time * 1000);
    }
}

/**
 * Tick callback using monotonic clock
 * @return elapsed milliseconds
 */
static uint32_t tick_get_cb(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t time_ms = t.tv_sec * 1000 + (t.tv_nsec / 1000000);
    return (uint32_t)time_ms;
}

#endif /* LV_USE_LINUX_FBDEV */
