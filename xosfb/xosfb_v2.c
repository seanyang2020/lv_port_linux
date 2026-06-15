/**
 * @file xosfb_v2.c
 *
 * XOS Framebuffer Library v2 — Implementation
 *
 * Extends xosfb v1 with hardware-accelerated 2D operations.
 * Uses libmpi.a for TDE2 (fill/copy/blend) and VGS2 (scale/convert/rotate).
 *
 * Hardware init order:  SYS → VB → VO → FB → TDE2 → VGS2
 * Hardware exit order:  VGS2 → TDE2 → FB → VO → SYS → VB
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

/* --- MPI headers (from libmpi.a) --- */
#include "fh_defines.h"
#include "fh_common.h"
#include "fh_system_mpi.h"
#include "fh_system_mpipara.h"
#include "fh_vb_mpi.h"
#include "fh_vb_mpipara.h"
#include "fh_vo_mpi.h"
#include "fh_tde_mpi.h"
#include "fh_tde_mpipara.h"
#include "fh_vgs2_mpi.h"
#include "fh_vgs2_mpipara.h"
#include "fb_drv_ioc.h"
#include "sample_comm.h"

#include "xosfb.h"
#include "xosfb_v2.h"

/*********************
 *      DEFINES
 *********************/

#define XOSFB_DEV_FB0          "/dev/fb0"
#define XOSFB_VO_DEV_DHD0      0

/* Max DMA blocks tracked for cleanup */
#define XOSFB_V2_MAX_DMA_BLKS 32

/**********************
 *      TYPEDEFS
 **********************/

/** Extended context — v1 fields first, then v2 additions */
struct xosfb_ctx {
    /* === v1 fields === */
    int fd;
    void *fbp;
    int width;
    int height;
    int bpp;
    int line_length;
    size_t screensize;
    xosfb_pixel_format_t fmt;
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    /* === v2 fields === */
    uint32_t caps;              /* Hardware capability flags */
    int      tde_opened;        /* TDE2 opened flag */
    int      vgs_opened;        /* VGS2 initialized flag */
    unsigned long fb_phy;       /* Cached FB physical address */

    /* DMA block tracking */
    VB_BLK   dma_blks[XOSFB_V2_MAX_DMA_BLKS];
    int      dma_blk_cnt;
};

/**********************
 *  STATIC PROTOTYPES
 **********************/

static int  sys_init_v2(void);
static void sys_exit_v2(void);
static int  vo_init_v2(int width, int height);
static void vo_exit_v2(void);
static int  tde_init(xosfb_ctx_t *ctx);
static void tde_exit(xosfb_ctx_t *ctx);
static int  vgs_init(xosfb_ctx_t *ctx);
static void vgs_exit(xosfb_ctx_t *ctx);
static void set_bitfields(struct fb_var_screeninfo *var, xosfb_pixel_format_t fmt);
static int  vgs_pixel_fmt(xosfb_pixel_format_t fmt);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

xosfb_ctx_t *xosfb_v2_init(int width, int height, xosfb_pixel_format_t fmt)
{
    int ret;
    int bShow;

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "xosfb_v2_init: invalid resolution %dx%d\n", width, height);
        return NULL;
    }
    if (fmt < 0 || fmt >= XOSFB_FMT_COUNT) {
        fprintf(stderr, "xosfb_v2_init: invalid format %d, defaulting to ARGB8888\n", fmt);
        fmt = XOSFB_FMT_ARGB8888;
    }

    /* Allocate extended context */
    xosfb_ctx_t *ctx = calloc(1, sizeof(xosfb_ctx_t));
    if (!ctx) {
        perror("xosfb_v2_init: calloc");
        return NULL;
    }
    ctx->fd = -1;
    ctx->fmt = fmt;
    ctx->width = width;
    ctx->height = height;
    ctx->caps = 0;

    /* Step 1: MPP System Init (VB pools + SYS) */
    ret = sys_init_v2();
    if (ret != 0) {
        fprintf(stderr, "xosfb_v2_init: sys_init_v2 failed (%d)\n", ret);
        goto fail_ctx;
    }

    /* Step 2: VO Device Init */
    ret = vo_init_v2(width, height);
    if (ret != 0) {
        fprintf(stderr, "xosfb_v2_init: vo_init_v2 failed (%d)\n", ret);
        goto fail_sys;
    }

    /* Step 3: FB Device Init (same as v1) */
    ctx->fd = open(XOSFB_DEV_FB0, O_RDWR);
    if (ctx->fd < 0) {
        perror("xosfb_v2_init: open " XOSFB_DEV_FB0);
        goto fail_vo;
    }
    printf("xosfb_v2: opened %s, fd=%d, %dx%d\n", XOSFB_DEV_FB0, ctx->fd, width, height);

    /* Hide during config */
    bShow = 0;
    ioctl(ctx->fd, FBIOPUT_SHOW_FYFB, &bShow); /* non-fatal */

    /* Get & configure var info */
    if (ioctl(ctx->fd, FBIOGET_VSCREENINFO, &ctx->var) < 0) {
        perror("xosfb_v2_init: FBIOGET_VSCREENINFO");
        goto fail_fb;
    }
    ctx->var.xres_virtual = width;
    ctx->var.yres_virtual = height;
    ctx->var.xoffset = 0;
    ctx->var.yoffset = 0;
    ctx->var.xres = width;
    ctx->var.yres = height;
    set_bitfields(&ctx->var, fmt);
    ctx->var.activate = FB_ACTIVATE_NOW;

    if (ioctl(ctx->fd, FBIOPUT_VSCREENINFO, &ctx->var) < 0) {
        perror("xosfb_v2_init: FBIOPUT_VSCREENINFO");
        goto fail_fb;
    }

    /* Get fix info */
    if (ioctl(ctx->fd, FBIOGET_FSCREENINFO, &ctx->fix) < 0) {
        perror("xosfb_v2_init: FBIOGET_FSCREENINFO");
        goto fail_fb;
    }
    ctx->line_length = ctx->fix.line_length;
    ctx->screensize   = ctx->fix.smem_len;
    ctx->bpp          = ctx->var.bits_per_pixel;
    ctx->fb_phy       = ctx->fix.smem_start;

    /* Configure layer: BUF_NONE mode */
    {
        FYFB_LAYER_INFO_S stLayerInfo;
        if (ioctl(ctx->fd, FBIOGET_LAYER_INFO, &stLayerInfo) == 0) {
            stLayerInfo.u32Mask = 0;
            stLayerInfo.BufMode = FYFB_LAYER_BUF_NONE;
            stLayerInfo.u32Mask |= FYFB_LAYERMASK_BUFMODE;
            ioctl(ctx->fd, FBIOPUT_LAYER_INFO, &stLayerInfo); /* non-fatal */
        }
    }

    /* Enable FB compression */
    {
        int bEnable = 1;
        if (ioctl(ctx->fd, FBIOPUT_COMPRESSION_FYFB, &bEnable) == 0) {
            ctx->caps |= XOSFB_V2_CAP_COMPRESS;
            printf("xosfb_v2: FB compression enabled\n");
        }
    }

    /* mmap the framebuffer */
    ctx->fbp = mmap(NULL, ctx->screensize, PROT_READ | PROT_WRITE,
                    MAP_SHARED, ctx->fd, 0);
    if (ctx->fbp == MAP_FAILED) {
        perror("xosfb_v2_init: mmap");
        goto fail_fb;
    }
    printf("xosfb_v2: mmap'd %zu bytes at %p, phy=0x%lx\n",
           ctx->screensize, ctx->fbp, ctx->fb_phy);

    /* Show the layer */
    bShow = 1;
    if (ioctl(ctx->fd, FBIOPUT_SHOW_FYFB, &bShow) < 0) {
        perror("xosfb_v2_init: FBIOPUT_SHOW_FYFB(show)");
        goto fail_mmap;
    }

    /* Step 4: TDE2 Init (2D acceleration) */
    ret = tde_init(ctx);
    if (ret == 0) {
        ctx->caps |= (XOSFB_V2_CAP_FILL | XOSFB_V2_CAP_COPY | XOSFB_V2_CAP_BLEND);
    } else {
        fprintf(stderr, "xosfb_v2_init: TDE2 init failed (%d), "
                "fill/copy/blend unavailable\n", ret);
    }

    /* Step 5: VGS2 Init (scale/convert/rotate) */
    ret = vgs_init(ctx);
    if (ret == 0) {
        ctx->caps |= (XOSFB_V2_CAP_SCALE | XOSFB_V2_CAP_CONVERT | XOSFB_V2_CAP_ROTATE);
    } else {
        fprintf(stderr, "xosfb_v2_init: VGS2 init failed (%d), "
                "scale/convert/rotate unavailable\n", ret);
    }

    printf("xosfb_v2: init complete, %dx%d bpp=%d caps=0x%x fmt=%d\n",
           width, height, ctx->bpp, ctx->caps, fmt);

    return ctx;

fail_mmap:
    munmap(ctx->fbp, ctx->screensize);
    ctx->fbp = NULL;
fail_fb:
    close(ctx->fd);
    ctx->fd = -1;
fail_vo:
    vo_exit_v2();
fail_sys:
    sys_exit_v2();
fail_ctx:
    free(ctx);
    return NULL;
}

void xosfb_v2_exit(xosfb_ctx_t *ctx)
{
    int i;
    if (!ctx) return;

    /* Free any remaining DMA blocks */
    for (i = 0; i < ctx->dma_blk_cnt; i++) {
        FH_VB_ReleaseBlock(ctx->dma_blks[i]);
    }
    ctx->dma_blk_cnt = 0;

    /* Step 5 reversed: VGS2 */
    vgs_exit(ctx);

    /* Step 4 reversed: TDE2 */
    tde_exit(ctx);

    /* Step 3 reversed: FB */
    if (ctx->fd >= 0) {
        int bShow = 0;
        ioctl(ctx->fd, FBIOPUT_SHOW_FYFB, &bShow);
    }
    if (ctx->fbp && ctx->fbp != MAP_FAILED) {
        munmap(ctx->fbp, ctx->screensize);
        ctx->fbp = NULL;
    }
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    /* Step 2 reversed: VO */
    vo_exit_v2();

    /* Step 1 reversed: SYS */
    sys_exit_v2();

    printf("xosfb_v2: exit complete\n");
    free(ctx);
}

/* ================================================================
 *  Capability Query
 * ================================================================ */

uint32_t xosfb_v2_get_caps(xosfb_ctx_t *ctx)
{
    return ctx ? ctx->caps : 0;
}

/* ================================================================
 *  TDE2: Hardware Fill
 * ================================================================ */

int xosfb_v2_fill_rect(xosfb_ctx_t *ctx, int x, int y, int w, int h, uint32_t color)
{
    if (!ctx || !ctx->fbp || ctx->fd < 0) return -EINVAL;
    if (!(ctx->caps & XOSFB_V2_CAP_FILL))   return -ENODEV;

    /* Clip to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ctx->width)  w = ctx->width  - x;
    if (y + h > ctx->height) h = ctx->height - y;
    if (w <= 0 || h <= 0) return 0; /* nothing to fill */

    TDE2_SURFACE_S dst_sf;
    memset(&dst_sf, 0, sizeof(dst_sf));
    dst_sf.u32PhyAddr  = (FH_UINT32)ctx->fb_phy;
    dst_sf.u32Width    = ctx->width;
    dst_sf.u32Height   = ctx->height;
    dst_sf.u32Stride   = ctx->line_length;
    dst_sf.bAlphaMax255 = FH_TRUE;
    dst_sf.bAlphaExt1555 = (ctx->fmt == XOSFB_FMT_ARGB1555) ? FH_TRUE : FH_FALSE;
    dst_sf.u8Alpha0    = 0;
    dst_sf.u8Alpha1    = 0xFF;

    switch (ctx->fmt) {
    case XOSFB_FMT_ARGB8888: dst_sf.enColorFmt = TDE2_COLOR_FMT_ARGB8888; break;
    case XOSFB_FMT_ARGB1555: dst_sf.enColorFmt = TDE2_COLOR_FMT_ARGB1555; break;
    case XOSFB_FMT_ARGB0565: dst_sf.enColorFmt = TDE2_COLOR_FMT_ARGB1555; break;
    default:                 dst_sf.enColorFmt = TDE2_COLOR_FMT_ARGB8888; break;
    }

    TDE2_RECT_S dst_rect;
    dst_rect.s32Xpos   = x;
    dst_rect.s32Ypos   = y;
    dst_rect.u32Width  = w;
    dst_rect.u32Height = h;

    TDE_HANDLE handle = FH_TDE2_BeginJob();
    if (handle == FH_ERR_TDE_INVALID_HANDLE) {
        fprintf(stderr, "xosfb_v2_fill_rect: BeginJob failed\n");
        return -ENODEV;
    }

    int ret = FH_TDE2_QuickFill(handle, &dst_sf, &dst_rect, color);
    if (ret != FH_SUCCESS) {
        FH_TDE2_CancelJob(handle);
        fprintf(stderr, "xosfb_v2_fill_rect: QuickFill failed (%d)\n", ret);
        return -EIO;
    }

    ret = FH_TDE2_EndJob(handle, FH_TRUE, FH_TRUE, 2000);
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb_v2_fill_rect: EndJob failed (%d)\n", ret);
        return -EIO;
    }

    return 0;
}

/* ================================================================
 *  TDE2: Hardware Copy
 * ================================================================ */

int xosfb_v2_copy_rect(xosfb_ctx_t *ctx,
        int src_x, int src_y, int w, int h,
        int dst_x, int dst_y)
{
    if (!ctx || !ctx->fbp || ctx->fd < 0) return -EINVAL;
    if (!(ctx->caps & XOSFB_V2_CAP_COPY))   return -ENODEV;
    if (w <= 0 || h <= 0) return -EINVAL;

    TDE2_SURFACE_S src_sf, dst_sf;
    memset(&src_sf, 0, sizeof(src_sf));
    memset(&dst_sf, 0, sizeof(dst_sf));

    /* Both src and dst are in the same FB surface */
    src_sf.u32PhyAddr  = (FH_UINT32)ctx->fb_phy;
    src_sf.u32Width    = ctx->width;
    src_sf.u32Height   = ctx->height;
    src_sf.u32Stride   = ctx->line_length;
    src_sf.bAlphaMax255 = FH_TRUE;
    src_sf.u8Alpha0    = 0;
    src_sf.u8Alpha1    = 0xFF;

    switch (ctx->fmt) {
    case XOSFB_FMT_ARGB8888: src_sf.enColorFmt = TDE2_COLOR_FMT_ARGB8888; break;
    case XOSFB_FMT_ARGB1555: src_sf.enColorFmt = TDE2_COLOR_FMT_ARGB1555; break;
    case XOSFB_FMT_ARGB0565: src_sf.enColorFmt = TDE2_COLOR_FMT_ARGB1555; break;
    default:                 src_sf.enColorFmt = TDE2_COLOR_FMT_ARGB8888; break;
    }

    dst_sf = src_sf; /* same surface properties */

    TDE2_RECT_S src_rect, dst_rect;
    src_rect.s32Xpos   = src_x;
    src_rect.s32Ypos   = src_y;
    src_rect.u32Width  = w;
    src_rect.u32Height = h;

    dst_rect.s32Xpos   = dst_x;
    dst_rect.s32Ypos   = dst_y;
    dst_rect.u32Width  = w;
    dst_rect.u32Height = h;

    TDE_HANDLE handle = FH_TDE2_BeginJob();
    if (handle == FH_ERR_TDE_INVALID_HANDLE) {
        fprintf(stderr, "xosfb_v2_copy_rect: BeginJob failed\n");
        return -ENODEV;
    }

    int ret = FH_TDE2_QuickCopy(handle, &src_sf, &src_rect, &dst_sf, &dst_rect);
    if (ret != FH_SUCCESS) {
        FH_TDE2_CancelJob(handle);
        fprintf(stderr, "xosfb_v2_copy_rect: QuickCopy failed (%d)\n", ret);
        return -EIO;
    }

    ret = FH_TDE2_EndJob(handle, FH_TRUE, FH_TRUE, 2000);
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb_v2_copy_rect: EndJob failed (%d)\n", ret);
        return -EIO;
    }

    return 0;
}

/* ================================================================
 *  VGS2: Hardware Blit (format convert + scale + position)
 * ================================================================ */

int xosfb_v2_blit(xosfb_ctx_t *ctx, const xosfb_v2_blit_desc_t *desc)
{
    if (!ctx || !desc || !desc->src_buf)  return -EINVAL;
    if (!(ctx->caps & XOSFB_V2_CAP_CONVERT)) return -ENODEV;
    if (desc->src_w <= 0 || desc->src_h <= 0) return -EINVAL;
    if (desc->dst_w <= 0 || desc->dst_h <= 0) return -EINVAL;

    VGS2_HANDLE handle;
    VGS_V2_CVT_CTRL ctrl;
    int ret;

    memset(&ctrl, 0, sizeof(ctrl));

    /* Source: DMA buffer.
     * FH_MEM_INFO.base  = physical address (FH_PHYADDR)
     * FH_MEM_INFO.vbase = virtual address (FH_VOID *)
     * The caller must pass the dma_buf.phy_addr for base,
     * and dma_buf.virt_addr for vbase.
     * For now we set base=0 (VGS2 can use vbase alone on some paths)
     * and pass the virtual address. */
    ctrl.src_data.data_yaddr.base  = 0;
    ctrl.src_data.data_yaddr.vbase = (FH_VOID *)desc->src_buf;
    ctrl.src_data.data_yaddr.size  = (FH_UINT32)(desc->src_h * desc->src_stride *
        (desc->src_fmt == XOSFB_FMT_ARGB8888 ? 4 : 2));
    ctrl.src_data.data_caddr.base  = 0;
    ctrl.src_data.data_caddr.size  = 0;
    ctrl.src_data.image_size.u32Width  = desc->src_w;
    ctrl.src_data.image_size.u32Height = desc->src_h;
    ctrl.src_data.start.u32X = 0;
    ctrl.src_data.start.u32Y = 0;

    /* Destination: framebuffer */
    ctrl.dst_data.data_yaddr.base  = (FH_PHYADDR)ctx->fb_phy;
    ctrl.dst_data.data_yaddr.vbase = (FH_VOID *)ctx->fbp;
    ctrl.dst_data.data_yaddr.size  = (FH_UINT32)ctx->screensize;
    ctrl.dst_data.data_caddr.base  = 0;
    ctrl.dst_data.data_caddr.size  = 0;
    ctrl.dst_data.image_size.u32Width  = ctx->width;
    ctrl.dst_data.image_size.u32Height = ctx->height;
    ctrl.dst_data.start.u32X = desc->dst_x;
    ctrl.dst_data.start.u32Y = desc->dst_y;

    ctrl.op_size.u32Width  = desc->dst_w;
    ctrl.op_size.u32Height = desc->dst_h;
    ctrl.src_format = vgs_pixel_fmt(desc->src_fmt);
    ctrl.src_comp   = 3; /* Y + C both present for RGB */

    ret = FH_VGS_V2_CVT(&handle, &ctrl, FH_TRUE); /* instant = block until done */
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb_v2_blit: FH_VGS_V2_CVT failed (%d)\n", ret);
        return -EIO;
    }

    return 0;
}

/* ================================================================
 *  VGS2: Hardware Rotate + Blit
 * ================================================================ */

int xosfb_v2_rotate_blit(xosfb_ctx_t *ctx,
        const void *src_buf, int src_w, int src_h,
        xosfb_pixel_format_t src_fmt,
        int dst_x, int dst_y, xosfb_v2_rotation_t rotation)
{
    if (!ctx || !src_buf)                      return -EINVAL;
    if (!(ctx->caps & XOSFB_V2_CAP_ROTATE))    return -ENODEV;
    if (src_w <= 0 || src_h <= 0)              return -EINVAL;

    VGS2_HANDLE handle;
    VGS_V2_ROT_CTRL ctrl;
    int ret;

    memset(&ctrl, 0, sizeof(ctrl));

    /* Source image */
    ctrl.src_data.data_addr.base  = 0;
    ctrl.src_data.data_addr.vbase = (FH_VOID *)src_buf;
    ctrl.src_data.data_addr.size  = (FH_UINT32)(src_w * src_h *
        (src_fmt == XOSFB_FMT_ARGB8888 ? 4 : 2));
    ctrl.src_data.data_addr_u.base = 0;
    ctrl.src_data.data_addr_u.size = 0;
    ctrl.src_data.data_addr_v.base = 0;
    ctrl.src_data.data_addr_v.size = 0;
    ctrl.src_data.image_size.u32Width  = src_w;
    ctrl.src_data.image_size.u32Height = src_h;
    ctrl.src_data.start.u32X = 0;
    ctrl.src_data.start.u32Y = 0;

    /* Destination */
    ctrl.dst_data.data_addr.base  = (FH_PHYADDR)ctx->fb_phy;
    ctrl.dst_data.data_addr.vbase = (FH_VOID *)ctx->fbp;
    ctrl.dst_data.data_addr.size  = (FH_UINT32)ctx->screensize;
    ctrl.dst_data.data_addr_u.base = 0;
    ctrl.dst_data.data_addr_u.size = 0;
    ctrl.dst_data.data_addr_v.base = 0;
    ctrl.dst_data.data_addr_v.size = 0;
    ctrl.dst_data.image_size.u32Width  = ctx->width;
    ctrl.dst_data.image_size.u32Height = ctx->height;
    ctrl.dst_data.start.u32X = dst_x;
    ctrl.dst_data.start.u32Y = dst_y;

    /* Output size: swap w/h for 90/270 */
    if (rotation == XOSFB_V2_ROTATE_90 || rotation == XOSFB_V2_ROTATE_270) {
        ctrl.op_size.u32Width  = src_h;
        ctrl.op_size.u32Height = src_w;
    } else {
        ctrl.op_size.u32Width  = src_w;
        ctrl.op_size.u32Height = src_h;
    }

    ctrl.src_format = vgs_pixel_fmt(src_fmt);
    ctrl.src_comp   = 3;
    ctrl.src_pixw   = src_w;
    ctrl.rotate     = (FH_ROTATE_OPS)rotation;
    ctrl.rotmode    = 0;

    ret = FH_VGS_V2_ROT(&handle, &ctrl, FH_TRUE); /* instant */
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb_v2_rotate_blit: FH_VGS_V2_ROT failed (%d)\n", ret);
        return -EIO;
    }

    return 0;
}

/* ================================================================
 *  DMA Buffer Management (MMZ via VB pools)
 * ================================================================ */

int xosfb_v2_alloc_dma(xosfb_ctx_t *ctx, unsigned int size, xosfb_v2_dma_buf_t *buf)
{
    VB_POOL pool;

    if (!ctx || !buf || size == 0) return -EINVAL;

    memset(buf, 0, sizeof(*buf));

    /* Allocate from VB pools.  VB_INVALID_POOLID = auto-select pool. */
    VB_BLK blk = FH_VB_GetBlock(VB_INVALID_POOLID, size, NULL);
    if (blk == 0) {
        fprintf(stderr, "xosfb_v2_alloc_dma: FH_VB_GetBlock(%u) failed\n", size);
        return -ENOMEM;
    }

    buf->phy_addr  = FH_VB_Handle2PhysAddr(blk);
    buf->size      = size;

    /* Get virtual address via pool ID + physical address */
    pool = FH_VB_Handle2PoolId(blk);
    FH_VB_GetBlkVirAddr(pool, buf->phy_addr, (FH_VOID **)&buf->virt_addr);

    if (!buf->phy_addr || !buf->virt_addr) {
        fprintf(stderr, "xosfb_v2_alloc_dma: invalid addr from VB block\n");
        FH_VB_ReleaseBlock(blk);
        return -ENOMEM;
    }

    /* Track block handle for cleanup */
    if (ctx->dma_blk_cnt < XOSFB_V2_MAX_DMA_BLKS) {
        ctx->dma_blks[ctx->dma_blk_cnt++] = blk;
    } else {
        fprintf(stderr, "xosfb_v2_alloc_dma: dma_blks table full (max %d)\n",
                XOSFB_V2_MAX_DMA_BLKS);
        FH_VB_ReleaseBlock(blk);
        return -ENOMEM;
    }

    return 0;
}

void xosfb_v2_free_dma(xosfb_ctx_t *ctx, xosfb_v2_dma_buf_t *buf)
{
    int i;

    if (!ctx || !buf || !buf->virt_addr) return;

    /* Find and release the VB block matching this buffer's phys addr */
    for (i = 0; i < ctx->dma_blk_cnt; i++) {
        VB_BLK blk = ctx->dma_blks[i];
        if (FH_VB_Handle2PhysAddr(blk) == (FH_PHYADDR)buf->phy_addr) {
            FH_VB_ReleaseBlock(blk);
            /* Remove from tracking array */
            ctx->dma_blks[i] = ctx->dma_blks[--ctx->dma_blk_cnt];
            memset(buf, 0, sizeof(*buf));
            return;
        }
    }

    fprintf(stderr, "xosfb_v2_free_dma: block not found for phy=0x%lx\n",
            buf->phy_addr);
}

/* ================================================================
 *   STATIC FUNCTIONS — System Init
 * ================================================================ */

static int sys_init_v2(void)
{
    int ret;
    VB_CONF_S vb_conf;

    memset(&vb_conf, 0, sizeof(vb_conf));
    vb_conf.u32MaxPoolCnt = 128;

    /* Pool 0: default blocks for intermediate buffers (32KB each ×4) */
    vb_conf.astCommPool[0].u32BlkSize = 32768;
    vb_conf.astCommPool[0].u32BlkCnt  = 4;

    ret = FH_VB_SetConf(&vb_conf);
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "sys_init_v2: FH_VB_SetConf failed (%d)\n", ret);
        return -1;
    }

    ret = FH_VB_Init();
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "sys_init_v2: FH_VB_Init failed (%d)\n", ret);
        return -1;
    }

    /* SYS Init — no explicit SetConf needed; alignment is auto-handled */
    ret = FH_SYS_Init();
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "sys_init_v2: FH_SYS_Init failed (%d)\n", ret);
        return -1;
    }

    printf("xosfb_v2: MPP system initialized\n");
    return 0;
}

static void sys_exit_v2(void)
{
    int i;

    FH_SYS_Exit();

    for (i = 0; i < VB_UID_BUTT; i++) {
        FH_VB_ExitModCommPool((VB_UID_E)i);
    }
    for (i = 0; i < VB_MAX_POOLS; i++) {
        FH_VB_DestroyPool((VB_POOL)i);
    }
    FH_VB_Exit();

    printf("xosfb_v2: MPP system exited\n");
}

/* ================================================================
 *   STATIC FUNCTIONS — VO Init
 * ================================================================ */

static int vo_init_v2(int width, int height)
{
    VO_PUB_ATTR_S attr;
    VO_DEV dev = XOSFB_VO_DEV_DHD0;
    int ret;

    memset(&attr, 0, sizeof(attr));

    /* Try to get display size from hardware first */
    USER_SYNC_INFO_S sync;
    ret = FH_VO_GetDispSize(dev, &sync);
    if (ret == FH_SUCCESS && sync.width > 0 && sync.height > 0) {
        attr.stUserSync.width     = sync.width;
        attr.stUserSync.height    = sync.height;
        attr.stUserSync.framerate  = sync.framerate;
    } else {
        /* Fallback: use user-specified dimensions */
        attr.stUserSync.width     = width;
        attr.stUserSync.height    = height;
        attr.stUserSync.framerate  = 60;
    }

    attr.enIntfSync  = VO_OUTPUT_USER;
    attr.enIntfType  = VO_INTF_LCD;
    attr.u32BgColor  = 0x00FFFFFF;

    printf("xosfb_v2: VO_Init DHD0 w=%d h=%d\n",
           attr.stUserSync.width, attr.stUserSync.height);

    ret = FH_VO_SetPubAttr(dev, &attr);
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb_v2: FH_VO_SetPubAttr failed (%d)\n", ret);
        return -1;
    }

    ret = FH_VO_Enable(dev);
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb_v2: FH_VO_Enable failed (%d)\n", ret);
        return -1;
    }

    return 0;
}

static void vo_exit_v2(void)
{
    int ret = FH_VO_Disable(XOSFB_VO_DEV_DHD0);
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb_v2: FH_VO_Disable failed (%d)\n", ret);
    }
    printf("xosfb_v2: VO exited\n");
}

/* ================================================================
 *   STATIC FUNCTIONS — TDE2 Init/Exit
 * ================================================================ */

static int tde_init(xosfb_ctx_t *ctx)
{
    (void)ctx;
    int ret = FH_TDE2_Open();
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb_v2: FH_TDE2_Open failed (%d)\n", ret);
        return -1;
    }
    ctx->tde_opened = 1;
    printf("xosfb_v2: TDE2 opened\n");
    return 0;
}

static void tde_exit(xosfb_ctx_t *ctx)
{
    if (ctx->tde_opened) {
        FH_TDE2_Close();
        ctx->tde_opened = 0;
        printf("xosfb_v2: TDE2 closed\n");
    }
}

/* ================================================================
 *   STATIC FUNCTIONS — VGS2 Init/Exit
 * ================================================================ */

static int vgs_init(xosfb_ctx_t *ctx)
{
    (void)ctx;
    /* FH_VGS_V2_Init is declared extern in the MPI headers.
     * If the linker can't find it, guard with #ifdef or use weak symbol. */
    extern FH_SINT32 FH_VGS_V2_Init(FH_VOID);
    int ret = FH_VGS_V2_Init();
    if (ret != FH_SUCCESS) {
        fprintf(stderr, "xosfb_v2: FH_VGS_V2_Init failed (%d)\n", ret);
        return -1;
    }
    ctx->vgs_opened = 1;
    printf("xosfb_v2: VGS2 initialized\n");
    return 0;
}

static void vgs_exit(xosfb_ctx_t *ctx)
{
    /* VGS2 has no explicit close/exit in this SDK version */
    ctx->vgs_opened = 0;
    printf("xosfb_v2: VGS2 exited\n");
}

/* ================================================================
 *   STATIC HELPERS
 * ================================================================ */

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
        var->bits_per_pixel = 32;
        var->transp.offset  = 24; var->transp.length  = 8; var->transp.msb_right = 0;
        var->red.offset     = 16; var->red.length     = 8; var->red.msb_right    = 0;
        var->green.offset   =  8; var->green.length   = 8; var->green.msb_right  = 0;
        var->blue.offset    =  0; var->blue.length    = 8; var->blue.msb_right   = 0;
        break;
    }
}

/**
 * Map xosfb_pixel_format_t to FH_VPU_VO_MODE (used by VGS2 as src_format)
 *
 * These values come from the FYFB_FMT_* enum in fb_drv_ioc.h,
 * reused as FH_VPU_VO_MODE in the VGS2 context:
 *   0 = RGB565, 5 = ARGB1555, 6 = ARGB8888
 */
static int vgs_pixel_fmt(xosfb_pixel_format_t fmt)
{
    switch (fmt) {
    case XOSFB_FMT_ARGB8888:  return 6;  /* FYFB_FMT_ARGB8888 */
    case XOSFB_FMT_ARGB1555:  return 5;  /* FYFB_FMT_ARGB1555 */
    case XOSFB_FMT_ARGB0565:  return 0;  /* FYFB_FMT_RGB565 */
    default:                  return 6;
    }
}
