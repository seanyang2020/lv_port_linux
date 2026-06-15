/**
 * @file xosfb_v2.h
 *
 * XOS Framebuffer Library v2 — Self-Contained Public API
 *
 * Standalone framebuffer + hardware-acceleration library for qm10xd.
 *
 * Hardware modules:
 *   - TDE2:  QuickFill, QuickCopy (2D acceleration)
 *   - VGS2:  CSC (color convert), Scale, Rotate (if driver loaded)
 *   - VB:    MMZ DMA buffer allocation
 *   - FB:    /dev/fb0 mmap + pan_display
 *
 * VGS2 is optional — if the driver is not loaded, init logs a warning
 * and the caps will not include SCALE/CONVERT/ROTATE.  blit/rotate_blit
 * will return -ENODEV.  fill_rect / copy_rect / alloc_dma are unaffected.
 *
 * Link: libxosfb_v2.a  (self-contained; also needs -lpthread -lm -ldl)
 */

#ifndef XOSFB_V2_H
#define XOSFB_V2_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      TYPEDEFS
 * *********************/

/** Pixel format selection */
typedef enum {
    XOSFB_FMT_ARGB8888 = 0,  /**< A:R:G:B = 8:8:8:8, 32bpp (default) */
    XOSFB_FMT_ARGB1555 = 1,  /**< A:R:G:B = 1:5:5:5, 16bpp */
    XOSFB_FMT_ARGB0565 = 2,  /**< A:R:G:B = 0:5:6:5, 16bpp (RGB565, no alpha) */
    XOSFB_FMT_COUNT           /**< Sentinel, must be last */
} xosfb_pixel_format_t;

/** Opaque context handle */
typedef struct xosfb_ctx xosfb_ctx_t;

/** Hardware acceleration capability flags (bitmask) */
typedef enum {
    XOSFB_V2_CAP_FILL       = 1 << 0,  /**< TDE2 hardware rectangle fill    */
    XOSFB_V2_CAP_COPY       = 1 << 1,  /**< TDE2 hardware rectangle copy    */
    XOSFB_V2_CAP_SCALE      = 1 << 2,  /**< VGS2 hardware scaling (optional) */
    XOSFB_V2_CAP_CONVERT    = 1 << 3,  /**< VGS2 color format convert (opt.) */
    XOSFB_V2_CAP_ROTATE     = 1 << 4,  /**< VGS2 hardware rotation (optional)*/
    XOSFB_V2_CAP_BLEND      = 1 << 5,  /**< TDE2 alpha blending             */
    XOSFB_V2_CAP_COMPRESS   = 1 << 6,  /**< FB DDR compression              */
} xosfb_v2_caps_t;

/** Rotation angle (VGS2, optional) */
typedef enum {
    XOSFB_V2_ROTATE_0   = 0,
    XOSFB_V2_ROTATE_90  = 1,
    XOSFB_V2_ROTATE_180 = 2,
    XOSFB_V2_ROTATE_270 = 3,
} xosfb_v2_rotation_t;

/** DMA buffer descriptor (physically contiguous, from MMZ) */
typedef struct {
    unsigned long  phy_addr;    /**< Physical address (for hardware)   */
    void          *virt_addr;   /**< Virtual address (for CPU access)   */
    unsigned int   size;        /**< Buffer size in bytes               */
} xosfb_v2_dma_buf_t;

/** Blit operation descriptor (VGS2, optional) */
typedef struct {
    const void          *src_buf;    /**< Source buffer (must be DMA memory!) */
    int                  src_w;      /**< Source width in pixels              */
    int                  src_h;      /**< Source height in pixels             */
    int                  src_stride; /**< Source line stride in pixels        */
    xosfb_pixel_format_t src_fmt;    /**< Source pixel format                 */

    int                  dst_x;      /**< Destination X offset in FB          */
    int                  dst_y;      /**< Destination Y offset in FB          */
    int                  dst_w;      /**< Destination width in pixels         */
    int                  dst_h;      /**< Destination height in pixels        */
} xosfb_v2_blit_desc_t;

/**********************
 *  Init / Exit / Caps
 * **********************/

xosfb_ctx_t *xosfb_v2_init(int width, int height, xosfb_pixel_format_t fmt);
void xosfb_v2_exit(xosfb_ctx_t *ctx);
uint32_t xosfb_v2_get_caps(xosfb_ctx_t *ctx);

/**********************
 *  TDE2: hardware fill & copy (always available if TDE2 driver loaded)
 * **********************/

int xosfb_v2_fill_rect(xosfb_ctx_t *ctx, int x, int y, int w, int h, uint32_t color);
int xosfb_v2_copy_rect(xosfb_ctx_t *ctx,
        int src_x, int src_y, int w, int h,
        int dst_x, int dst_y);

/**********************
 *  VGS2: blit & rotate (optional — returns -ENODEV if VGS2 not available)
 * **********************/

int xosfb_v2_blit(xosfb_ctx_t *ctx, const xosfb_v2_blit_desc_t *desc);
int xosfb_v2_rotate_blit(xosfb_ctx_t *ctx,
        const void *src_buf, int src_w, int src_h,
        xosfb_pixel_format_t src_fmt,
        int dst_x, int dst_y, xosfb_v2_rotation_t rotation);

/**********************
 *  DMA buffer management (MMZ / VB)
 * **********************/

int  xosfb_v2_alloc_dma(xosfb_ctx_t *ctx, unsigned int size, xosfb_v2_dma_buf_t *buf);
void xosfb_v2_free_dma(xosfb_ctx_t *ctx, xosfb_v2_dma_buf_t *buf);

/**********************
 *  FB accessors
 * **********************/

void *xosfb_get_fb_ptr(xosfb_ctx_t *ctx);
int xosfb_get_line_length(xosfb_ctx_t *ctx);
void xosfb_get_resolution(xosfb_ctx_t *ctx, int *w, int *h);
int xosfb_get_bpp(xosfb_ctx_t *ctx);
xosfb_pixel_format_t xosfb_get_pixel_format(xosfb_ctx_t *ctx);
void xosfb_pan_display(xosfb_ctx_t *ctx);
void xosfb_show(xosfb_ctx_t *ctx, int enable);

#ifdef __cplusplus
}
#endif

#endif /* XOSFB_V2_H */
