/**
 * @file js_tab.h
 *
 * JS Apps tab UI — enumerates JS applications and provides
 * launch / return flow.  All logic is gated on LV_USE_JS_ENGINE.
 */
#ifndef JS_TAB_H
#define JS_TAB_H

#include "lvgl/lvgl.h"
#include "lv_conf.h"

#if LV_USE_JS_ENGINE

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the JS Apps list UI inside @p parent.
 *
 * @param parent  parent object (typically a tab page)
 * @return the container object (the app-list screen)
 */
lv_obj_t * lv_js_tab_create(lv_obj_t * parent);

/**
 * Trigger a return from the currently-running JS app back to the
 * app list.  Safe to call from JS (via lvgljs.exit()) or from any
 * LVGL event handler.
 *
 * Does nothing if no JS app is currently running.
 */
void lv_js_tab_return(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_USE_JS_ENGINE */

#endif /* JS_TAB_H */
