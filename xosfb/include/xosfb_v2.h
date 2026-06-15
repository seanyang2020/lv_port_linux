/**
 * @file xosfb_v2.h
 *
 * XOS Framebuffer Library v2 — Public API
 *
 * Extends xosfb v1 with hardware-accelerated 2D operations using
 * TDE2 (2D Graphics Engine) and VGS2 (Video Graphics Sub-system)
 * via the libmpi.a MPI interface.
 *
 * v1 API (xosfb.h) is fully backward-compatible.
 * v2 adds GPU-accelerated fill, copy, blit, scale, and rotate.
 *
 * Hardware modules used:
 *   - TDE2:  QuickFill, QuickCopy, Bitblit (alpha blend)
 *   - VGS2:  CSC (color space convert), Scale, Rotate
 *   - VB:    MMZ physical memory allocation for intermediate buffers
 *
 * Dependencies:
 *   - libmpi.a  (FH_TDE2_*, FH_VGS_V2_*, FH_VB_*, FH_SYS_*)
 *   - linux/fb.h (framebuffer ioctl)
 *   - xosfb.h   (v1 base API)
 */

#ifndef XOSFB_V2_H
#define XOSFB_V2_H

#include "xosfb.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      TYPEDEFS
 *********************/

/** Hardware acceleration capability flags (bitmask) */
typedef enum {
    XOSFB_V2_CAP_FILL       = 1 << 0,  /**< TDE2 hardware rectangle fill    */
    XOSFB_V2_CAP_COPY       = 1 << 1,  /**< TDE2 hardware rectangle copy    */
    XOSFB_V2_CAP_SCALE      = 1 << 2,  /**< VGS2 hardware scaling            */
    XOSFB_V2_CAP_CONVERT    = 1 << 3,  /**< VGS2 color format conversion    */
    XOSFB_V2_CAP_ROTATE     = 1 << 4,  /**< VGS2 hardware rotation           */
    XOSFB_V2_CAP_BLEND      = 1 << 5,  /**< TDE2 alpha blending             */
    XOSFB_V2_CAP_COMPRESS   = 1 << 6,  /**< FB DDR compression              */
    XOSFB_V2_CAP_ALL        = 0x7F,    /**< All capabilities                 */
} xosfb_v2_caps_t;

/** Rotation angle for hardware rotate */
typedef enum {
    XOSFB_V2_ROTATE_0   = 0,
    XOSFB_V2_ROTATE_90  = 1,
    XOSFB_V2_ROTATE_180 = 2,
    XOSFB_V2_ROTATE_270 = 3,
} xosfb_v2_rotation_t;

/** DMA buffer descriptor (MMZ-allocated, physically contiguous) */
typedef struct {
    unsigned long  phy_addr;    /**< Physical address (for hardware)   */
    void          *virt_addr;   /**< Virtual address (for CPU access)   */
    unsigned int   size;        /**< Buffer size in bytes               */
} xosfb_v2_dma_buf_t;

/** Blit operation descriptor */
typedef struct {
    const void    *src_buf;     /**< Source buffer (NULL = use FB)      */
    int            src_w;       /**< Source width in pixels              */
    int            src_h;       /**< Source height in pixels             */
    int            src_stride;  /**< Source line stride in pixels        */
    xosfb_pixel_format_t src_fmt; /**< Source pixel format               */

    int            dst_x;       /**< Destination X offset in FB          */
    int            dst_y;       /**< Destination Y offset in FB          */
    int            dst_w;       /**< Destination width in pixels         */
    int            dst_h;       /**< Destination height in pixels        */
} xosfb_v2_blit_desc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/* ================================================================
 *  Initialization (replaces xosfb_init for v2 features)
 * ================================================================ */

/**
 * Initialize display + hardware acceleration (TDE2 + VGS2)
 *
 * This is the v2 entry point. Compared to xosfb_init():
 *   - Properly initializes MPP system (VB pools + SYS)
 *   - Opens TDE2 device for 2D acceleration
 *   - Initializes VGS2 device for scaling/rotation/conversion
 *
 * @param width   screen width in pixels
 * @param height  screen height in pixels
 * @param fmt     pixel format (XOSFB_FMT_ARGB8888 or XOSFB_FMT_ARGB1555)
 * @return context handle, or NULL on failure
 */
xosfb_ctx_t *xosfb_v2_init(int width, int height, xosfb_pixel_format_t fmt);

/**
 * Tear down hardware acceleration and display
 * @param ctx context handle from xosfb_v2_init()
 */
void xosfb_v2_exit(xosfb_ctx_t *ctx);

/* ================================================================
 *  Capability Query
 * ================================================================ */

/**
 * Query supported hardware acceleration capabilities.
 * Always call after xosfb_v2_init() — result is a compile-time
 * constant for a given chip, but runtime query allows graceful
 * fallback if a hardware module fails to initialize.
 *
 * @param ctx  context handle
 * @return bitmask of xosfb_v2_caps_t flags
 */
uint32_t xosfb_v2_get_caps(xosfb_ctx_t *ctx);

/* ================================================================
 *  TDE2 Hardware Accelerated Operations
 * ================================================================ */

/**
 * Hardware-accelerated rectangle fill (TDE2 QuickFill)
 *
 * Fills a rectangular region of the framebuffer with a solid color.
 * 10-50x faster than CPU memset/fill loops for large areas.
 *
 * @param ctx    context handle
 * @param x      left coordinate (pixels)
 * @param y      top coordinate (pixels)
 * @param w      width (pixels)
 * @param h      height (pixels)
 * @param color  fill color in the FB's native pixel format
 * @return 0 on success, negative on error
 */
int xosfb_v2_fill_rect(xosfb_ctx_t *ctx, int x, int y, int w, int h, uint32_t color);

/**
 * Hardware-accelerated rectangle copy (TDE2 QuickCopy)
 *
 * Copies a rectangular region within the framebuffer.
 * Useful for scroll, window move, or double-buffer patterns.
 *
 * @param ctx    context handle
 * @param src_x  source left coordinate
 * @param src_y  source top coordinate
 * @param w      width to copy
 * @param h      height to copy
 * @param dst_x  destination left coordinate
 * @param dst_y  destination top coordinate
 * @return 0 on success, negative on error
 */
int xosfb_v2_copy_rect(xosfb_ctx_t *ctx,
        int src_x, int src_y, int w, int h,
        int dst_x, int dst_y);

/* ================================================================
 *  VGS2 Hardware Accelerated Operations
 * ================================================================ */

/**
 * Hardware-accelerated blit with optional format conversion (VGS2 CSC)
 *
 * Blits a source buffer to the framebuffer. If src_fmt differs from
 * the FB's format, VGS2 performs hardware color-space conversion.
 * If src_w/src_h differ from dst_w/dst_h, hardware scaling is applied.
 *
 * Common use cases:
 *   - LVGL ARGB8888 buffer → FB ARGB1555: format conversion
 *   - Render at lower resolution → upscale to FB: scaling
 *   - Any combination of the above
 *
 * The source buffer must be in physically contiguous memory (MMZ).
 * Use xosfb_v2_alloc_dma() to allocate DMA-suitable buffers.
 *
 * @param ctx   context handle
 * @param desc  blit descriptor (src buffer, dimensions, formats, dst region)
 * @return 0 on success, negative on error
 */
int xosfb_v2_blit(xosfb_ctx_t *ctx, const xosfb_v2_blit_desc_t *desc);

/**
 * Hardware-accelerated rotation blit (VGS2 Rotate)
 *
 * Rotates the source buffer and blits the result to the framebuffer.
 * The destination region is automatically calculated based on rotation:
 *   - 90°:  src WxH → dst HxW at (dst_x, dst_y)
 *   - 270°: src WxH → dst HxW at (dst_x, dst_y)
 *   - 180°: src WxH → dst WxH at (dst_x, dst_y)
 *
 * @param ctx       context handle
 * @param src_buf   source buffer (must be DMA/MMZ memory)
 * @param src_w     source width
 * @param src_h     source height
 * @param src_fmt   source pixel format
 * @param dst_x     destination X offset in FB
 * @param dst_y     destination Y offset in FB
 * @param rotation  rotation angle
 * @return 0 on success, negative on error
 */
int xosfb_v2_rotate_blit(xosfb_ctx_t *ctx,
        const void *src_buf, int src_w, int src_h,
        xosfb_pixel_format_t src_fmt,
        int dst_x, int dst_y, xosfb_v2_rotation_t rotation);

/* ================================================================
 *  DMA Buffer Management (MMZ)
 * ================================================================ */

/**
 * Allocate a physically-contiguous DMA buffer from MMZ.
 *
 * Required for source buffers used with xosfb_v2_blit() and
 * xosfb_v2_rotate_blit() — VGS2 hardware needs physical addresses.
 *
 * @param ctx   context handle
 * @param size  size in bytes
 * @param buf   [out] allocated buffer descriptor
 * @return 0 on success, negative on error
 */
int xosfb_v2_alloc_dma(xosfb_ctx_t *ctx, unsigned int size, xosfb_v2_dma_buf_t *buf);

/**
 * Free a DMA buffer previously allocated by xosfb_v2_alloc_dma()
 * @param ctx context handle
 * @param buf buffer to free
 */
void xosfb_v2_free_dma(xosfb_ctx_t *ctx, xosfb_v2_dma_buf_t *buf);

/* ================================================================
 *  v1 Compatibility (available in v2 ctx as well)
 * ================================================================ */

/* All xosfb.h v1 functions work with xosfb_v2_init() context:
 *   xosfb_get_fb_ptr()
 *   xosfb_get_line_length()
 *   xosfb_get_resolution()
 *   xosfb_get_bpp()
 *   xosfb_get_pixel_format()
 *   xosfb_pan_display()
 *   xosfb_show()
 */

#ifdef __cplusplus
}
#endif

#endif /* XOSFB_V2_H */
