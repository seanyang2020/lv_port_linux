/**
 * @file xosfb.h
 *
 * XOS Framebuffer Library - Public API
 *
 * Self-contained framebuffer display library for qm10xd hardware.
 * Encapsulates all platform-specific initialization (SYS, VO, FB layer setup)
 * and exposes a minimal API for direct framebuffer rendering.
 */
#ifndef XOSFB_H
#define XOSFB_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      TYPEDEFS
 *********************/

/** Pixel format selection */
typedef enum {
    XOSFB_FMT_ARGB8888 = 0,  /**< A:R:G:B = 8:8:8:8, 32bpp (default) */
    XOSFB_FMT_ARGB1555 = 1,  /**< A:R:G:B = 1:5:5:5, 16bpp */
    XOSFB_FMT_ARGB0565 = 2,  /**< A:R:G:B = 0:5:6:5, 16bpp (RGB565, no alpha) */
    XOSFB_FMT_COUNT        /**< Sentinel, must be last */
} xosfb_pixel_format_t;

/** Opaque context handle */
typedef struct xosfb_ctx xosfb_ctx_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the display system: SYS + VO + FB open + layer setup + mmap
 *
 * @param width  screen width in pixels
 * @param height screen height in pixels
 * @param fmt    pixel format (default: XOSFB_FMT_ARGB8888)
 * @return context handle, or NULL on failure
 */
xosfb_ctx_t *xosfb_init(int width, int height, xosfb_pixel_format_t fmt);

/**
 * Tear down and release all resources
 * @param ctx context handle from xosfb_init()
 */
void xosfb_exit(xosfb_ctx_t *ctx);

/**
 * Get the mmap'd framebuffer pointer for direct pixel access
 * @param ctx context handle
 * @return pointer to framebuffer memory
 */
void *xosfb_get_fb_ptr(xosfb_ctx_t *ctx);

/**
 * Get framebuffer line stride in bytes
 * @param ctx context handle
 * @return bytes per line
 */
int xosfb_get_line_length(xosfb_ctx_t *ctx);

/**
 * Get screen resolution
 * @param ctx context handle
 * @param w   [out] width in pixels
 * @param h   [out] height in pixels
 */
void xosfb_get_resolution(xosfb_ctx_t *ctx, int *w, int *h);

/**
 * Get bits per pixel
 * @param ctx context handle
 * @return bpp (16 or 32)
 */
int xosfb_get_bpp(xosfb_ctx_t *ctx);

/**
 * Get current pixel format
 * @param ctx context handle
 * @return pixel format enum value
 */
xosfb_pixel_format_t xosfb_get_pixel_format(xosfb_ctx_t *ctx);

/**
 * Commit framebuffer content to display (hardware pan/flip)
 * @param ctx context handle
 */
void xosfb_pan_display(xosfb_ctx_t *ctx);

/**
 * Show or hide the framebuffer layer
 * @param ctx    context handle
 * @param enable 1 = show, 0 = hide
 */
void xosfb_show(xosfb_ctx_t *ctx, int enable);

#ifdef __cplusplus
}
#endif

#endif /* XOSFB_H */
