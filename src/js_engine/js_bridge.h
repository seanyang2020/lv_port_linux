/**
 * @file js_bridge.h
 *
 * JavaScript engine bridge — C-compatible API for QuickJS + txiki.js.
 *
 * All JS engine functionality is gated behind LV_USE_JS_ENGINE.
 * When the macro is 0, every function becomes a no-op stub so callers
 * don't need their own #if guards.
 */
#ifndef JS_BRIDGE_H
#define JS_BRIDGE_H

#include "lvgl/lvgl.h"
#include "lv_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#if LV_USE_JS_ENGINE

/**
 * Initialize the QuickJS runtime and register LVGL native bindings.
 * Must be called once before any other js_engine_* function.
 * @return 0 on success, -1 on error
 */
int js_engine_init(void);

/**
 * Load and execute a JavaScript bundle file.
 * The JS code renders LVGL widgets on the currently active screen.
 * @param script_path  absolute path to the index.js bundle
 * @return 0 on success, -1 on error
 */
int js_engine_run_script(const char *script_path);

/**
 * Drive the libuv event loop for one non-blocking tick.
 * Call this periodically (e.g. every 30 ms) from an LVGL timer
 * while a JS app is running.
 */
void js_engine_tick(void);

/**
 * Tear down the JS runtime and release all resources.
 * Call when returning from a JS app to the app list.
 */
void js_engine_cleanup(void);

/**
 * Check whether a JS app is currently executing.
 * @return 1 if running, 0 otherwise
 */
int js_engine_is_running(void);

#else  /* !LV_USE_JS_ENGINE — no-op stubs */

static inline int  js_engine_init(void)              { return -1; }
static inline int  js_engine_run_script(const char *p){ (void)p; return -1; }
static inline void js_engine_tick(void)              { }
static inline void js_engine_cleanup(void)           { }
static inline int  js_engine_is_running(void)        { return 0; }

#endif /* LV_USE_JS_ENGINE */

#ifdef __cplusplus
}
#endif

#endif /* JS_BRIDGE_H */
