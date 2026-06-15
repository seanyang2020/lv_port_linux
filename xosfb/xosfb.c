/**
 * @file xosfb.c
 *
 * XOS Framebuffer Library - Implementation
 *
 * Encapsulates qm10xd hardware-specific framebuffer initialization:
 *   SYS_Init → VO_Init → FB open → layer setup → compression → mmap
 *
 * Based on common_fb/fb_test.c
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include "fh_tde_mpi.h"
#include "fb_drv_ioc.h"
#include "sample_comm.h"

#include "xosfb.h"

/*********************
 *      DEFINES
 *********************/

#define XOSFB_DEV_FB0   "/dev/fb0"
#define XOSFB_DEV_FB4   "/dev/fb4"

/**********************
 *      TYPEDEFS
 **********************/

struct xosfb_ctx {
    int fd;                     /* framebuffer file descriptor */
    void *fbp;                  /* mmap'd framebuffer pointer */
    int width;
    int height;
    int bpp;
    int line_length;
    size_t screensize;
    xosfb_pixel_format_t fmt;
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
};

/**********************
 *  STATIC PROTOTYPES
 **********************/

static int sys_init(void);
static void sys_exit(void);
static int vo_init(int width, int height);
static void vo_exit(void);
static void set_bitfields(struct fb_var_screeninfo *var, xosfb_pixel_format_t fmt);

/**********************
 *  STATIC VARIABLES
 **********************/

/* Layer 0 (G0) uses /dev/fb0, Layer 1 (G1) uses /dev/fb4 */
/* Currently using GRAPHICS_LAYER_G0 (layer 0 → /dev/fb0) */

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

xosfb_ctx_t *xosfb_init(int width, int height, xosfb_pixel_format_t fmt)
{
    int ret;
    int bShow;

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "xosfb_init: invalid resolution %dx%d\n", width, height);
        return NULL;
    }

    if (fmt < 0 || fmt >= XOSFB_FMT_COUNT) {
        fprintf(stderr, "xosfb_init: invalid format %d, defaulting to ARGB8888\n", fmt);
        fmt = XOSFB_FMT_ARGB8888;
    }

    /* Allocate context */
    xosfb_ctx_t *ctx = calloc(1, sizeof(xosfb_ctx_t));
    if (!ctx) {
        perror("xosfb_init: calloc");
        return NULL;
    }
    ctx->fd = -1;
    ctx->fmt = fmt;
    ctx->width = width;
    ctx->height = height;

    /* Step 1: MPP system init */
    ret = sys_init();
    if (ret != 0) {
        fprintf(stderr, "xosfb_init: sys_init failed (%d)\n", ret);
        goto fail;
    }

    /* Step 2: VO device init */
    ret = vo_init(width, height);
    if (ret != 0) {
        fprintf(stderr, "xosfb_init: vo_init failed (%d)\n", ret);
        goto fail_sys;
    }

    /* Step 3: Open framebuffer device */
    ctx->fd = open(XOSFB_DEV_FB0, O_RDWR);
    if (ctx->fd < 0) {
        perror("xosfb_init: open " XOSFB_DEV_FB0);
        goto fail_vo;
    }
    printf("xosfb: opened %s, fd=%d, resolution=%dx%d\n",
           XOSFB_DEV_FB0, ctx->fd, width, height);

    /* Step 4: Hide layer before configuration */
    bShow = 0;
    if (ioctl(ctx->fd, FBIOPUT_SHOW_FYFB, &bShow) < 0) {
        perror("xosfb_init: FBIOPUT_SHOW_FYFB(hide)");
        /* Non-fatal */
    }

    /* Step 5: Get current variable info */
    if (ioctl(ctx->fd, FBIOGET_VSCREENINFO, &ctx->var) < 0) {
        perror("xosfb_init: FBIOGET_VSCREENINFO");
        goto fail_fb;
    }

    /* Step 6: Configure variable info with selected format */
    ctx->var.xres_virtual = width;
    ctx->var.yres_virtual = height;
    ctx->var.xoffset = 0;
    ctx->var.yoffset = 0;
    ctx->var.xres = width;
    ctx->var.yres = height;
    set_bitfields(&ctx->var, fmt);
    ctx->var.activate = FB_ACTIVATE_NOW;

    if (ioctl(ctx->fd, FBIOPUT_VSCREENINFO, &ctx->var) < 0) {
        perror("xosfb_init: FBIOPUT_VSCREENINFO");
        goto fail_fb;
    }

    /* Step 7: Get fixed info */
    if (ioctl(ctx->fd, FBIOGET_FSCREENINFO, &ctx->fix) < 0) {
        perror("xosfb_init: FBIOGET_FSCREENINFO");
        goto fail_fb;
    }

    ctx->line_length = ctx->fix.line_length;
    ctx->screensize = ctx->fix.smem_len;
    ctx->bpp = ctx->var.bits_per_pixel;

    /* Step 8: Configure layer info (BUF_NONE mode) */
    {
        FYFB_LAYER_INFO_S stLayerInfo;
        if (ioctl(ctx->fd, FBIOGET_LAYER_INFO, &stLayerInfo) < 0) {
            perror("xosfb_init: FBIOGET_LAYER_INFO");
            /* Non-fatal on some hardware */
        } else {
            stLayerInfo.u32Mask = 0;
            stLayerInfo.BufMode = FYFB_LAYER_BUF_NONE;
            stLayerInfo.u32Mask |= FYFB_LAYERMASK_BUFMODE;

            ret = ioctl(ctx->fd, FBIOPUT_LAYER_INFO, &stLayerInfo);
            if (ret < 0) {
                perror("xosfb_init: FBIOPUT_LAYER_INFO");
                /* Non-fatal */
            }
        }
    }

    /* Step 9: Enable compression */
    {
        int bEnable;
        ret = ioctl(ctx->fd, FBIOGET_COMPRESSION_FYFB, &bEnable);
        if (ret == 0) {
            printf("xosfb: compression currently %s\n", bEnable ? "enabled" : "disabled");
        }

        bEnable = 1;
        ret = ioctl(ctx->fd, FBIOPUT_COMPRESSION_FYFB, &bEnable);
        if (ret < 0) {
            perror("xosfb_init: FBIOPUT_COMPRESSION_FYFB");
            /* Non-fatal */
        } else {
            printf("xosfb: compression enabled\n");
        }
    }

    /* Step 10: mmap the framebuffer */
    ctx->fbp = mmap(NULL, ctx->screensize, PROT_READ | PROT_WRITE,
                    MAP_SHARED, ctx->fd, 0);
    if (ctx->fbp == MAP_FAILED) {
        perror("xosfb_init: mmap");
        goto fail_fb;
    }
    printf("xosfb: mmap'd %zu bytes at %p\n", ctx->screensize, ctx->fbp);

    /* Step 11: Show the layer */
    bShow = 1;
    if (ioctl(ctx->fd, FBIOPUT_SHOW_FYFB, &bShow) < 0) {
        perror("xosfb_init: FBIOPUT_SHOW_FYFB(show)");
        goto fail_mmap;
    }

    printf("xosfb: init complete, %dx%d bpp=%d line_length=%d fmt=%d\n",
           width, height, ctx->bpp, ctx->line_length, fmt);

    return ctx;

fail_mmap:
    munmap(ctx->fbp, ctx->screensize);
    ctx->fbp = NULL;
fail_fb:
    close(ctx->fd);
    ctx->fd = -1;
fail_vo:
    vo_exit();
fail_sys:
    sys_exit();
fail:
    free(ctx);
    return NULL;
}

void xosfb_exit(xosfb_ctx_t *ctx)
{
    if (!ctx) return;

    /* Hide the layer */
    if (ctx->fd >= 0) {
        int bShow = 0;
        ioctl(ctx->fd, FBIOPUT_SHOW_FYFB, &bShow);
    }

    /* Unmap framebuffer */
    if (ctx->fbp && ctx->fbp != MAP_FAILED) {
        munmap(ctx->fbp, ctx->screensize);
        ctx->fbp = NULL;
    }

    /* Close device */
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    /* VO exit */
    vo_exit();

    /* SYS exit */
    sys_exit();

    printf("xosfb: exit complete\n");
    free(ctx);
}

void *xosfb_get_fb_ptr(xosfb_ctx_t *ctx)
{
    return ctx ? ctx->fbp : NULL;
}

int xosfb_get_line_length(xosfb_ctx_t *ctx)
{
    return ctx ? ctx->line_length : 0;
}

void xosfb_get_resolution(xosfb_ctx_t *ctx, int *w, int *h)
{
    if (ctx) {
        *w = ctx->width;
        *h = ctx->height;
    } else {
        *w = *h = 0;
    }
}

int xosfb_get_bpp(xosfb_ctx_t *ctx)
{
    return ctx ? ctx->bpp : 0;
}

xosfb_pixel_format_t xosfb_get_pixel_format(xosfb_ctx_t *ctx)
{
    return ctx ? ctx->fmt : XOSFB_FMT_ARGB8888;
}

void xosfb_pan_display(xosfb_ctx_t *ctx)
{
    if (!ctx || ctx->fd < 0) return;

    ctx->var.yoffset = 0;
    if (ioctl(ctx->fd, FBIOPAN_DISPLAY, &ctx->var) < 0) {
        perror("xosfb_pan_display: FBIOPAN_DISPLAY");
    }
}

void xosfb_show(xosfb_ctx_t *ctx, int enable)
{
    if (!ctx || ctx->fd < 0) return;

    int bShow = enable ? 1 : 0;
    if (ioctl(ctx->fd, FBIOPUT_SHOW_FYFB, &bShow) < 0) {
        perror("xosfb_show: FBIOPUT_SHOW_FYFB");
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Set color bitfield layout based on pixel format
 *
 * The hardware supports ARGB8888, ARGB1555, and RGB565 (ARGB0565).
 * Each format maps red/green/blue/alpha to different bit positions.
 */
static void set_bitfields(struct fb_var_screeninfo *var, xosfb_pixel_format_t fmt)
{
    switch (fmt) {
        case XOSFB_FMT_ARGB8888:
            var->bits_per_pixel = 32;
            var->transp.offset  = 24; var->transp.length  = 8; var->transp.msb_right = 0;
            var->red.offset     = 16; var->red.length     = 8; var->red.msb_right    = 0;
            var->green.offset   =  8; var->green.length   = 8; var->green.msb_right  = 0;
            var->blue.offset    =  0; var->blue.length    = 8; var->blue.msb_right   = 0;
            break;

        case XOSFB_FMT_ARGB1555:
            var->bits_per_pixel = 16;
            var->transp.offset  = 15; var->transp.length  = 1; var->transp.msb_right = 0;
            var->red.offset     = 10; var->red.length     = 5; var->red.msb_right    = 0;
            var->green.offset   =  5; var->green.length   = 5; var->green.msb_right  = 0;
            var->blue.offset    =  0; var->blue.length    = 5; var->blue.msb_right   = 0;
            break;

        case XOSFB_FMT_ARGB0565:
            var->bits_per_pixel = 16;
            var->transp.offset  =  0; var->transp.length  = 0; var->transp.msb_right = 0;
            var->red.offset     = 11; var->red.length     = 5; var->red.msb_right    = 0;
            var->green.offset   =  5; var->green.length   = 6; var->green.msb_right  = 0;
            var->blue.offset    =  0; var->blue.length    = 5; var->blue.msb_right   = 0;
            break;

        default:
            /* Default to ARGB8888 */
            var->bits_per_pixel = 32;
            var->transp.offset  = 24; var->transp.length  = 8; var->transp.msb_right = 0;
            var->red.offset     = 16; var->red.length     = 8; var->red.msb_right    = 0;
            var->green.offset   =  8; var->green.length   = 8; var->green.msb_right  = 0;
            var->blue.offset    =  0; var->blue.length    = 8; var->blue.msb_right   = 0;
            break;
    }
}

/**
 * Initialize MPP system (VB pools, SYS)
 *
 * Currently a no-op on this platform — the system is pre-initialized
 * by the bootloader or earlier boot stages.
 */
static int sys_init(void)
{
    /* On qm10xd, the MPP system is pre-initialized.
     * If full init is needed, uncomment the VB/SYS init sequence
     * from common_fb/fb_test.c FB_COMM_SYS_Init().
     */
    return 0;
}

static void sys_exit(void)
{
    /* No-op: system stays initialized */
}

/**
 * Initialize VO device for framebuffer output
 */
static int vo_init(int width, int height)
{
    VO_DEV VoDev = SAMPLE_VO_DEV_DHD0;  /* DHD0 */
    VO_PUB_ATTR_S stVoPubAttr;

    memset(&stVoPubAttr, 0, sizeof(stVoPubAttr));
    stVoPubAttr.enIntfSync = VO_OUTPUT_USER;
    stVoPubAttr.enIntfType = VO_INTF_LCD;
    stVoPubAttr.u32BgColor = 0x00FFFFFF;
    stVoPubAttr.stUserSync.width = width;
    stVoPubAttr.stUserSync.height = height;
    stVoPubAttr.stUserSync.framerate = 60;

    printf("xosfb: VO_Init DHD0 w=%d h=%d\n", width, height);

    int ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb: SAMPLE_COMM_VO_StartDev failed (ret=%d)\n", ret);
        return -1;
    }

    return 0;
}

static void vo_exit(void)
{
    int ret = FH_VO_Disable(SAMPLE_VO_DEV_DHD0);
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb: FH_VO_Disable failed (ret=%d)\n", ret);
    }
}
