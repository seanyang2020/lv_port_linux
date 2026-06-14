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

/* ---- Debug logging: set to 1 to trace JS lifecycle events ---- */
#define JS_DEBUG 1

#if JS_DEBUG
#define JS_LOG(fmt, ...) LV_LOG_USER("[js-debug] " fmt, ##__VA_ARGS__)
#else
#define JS_LOG(fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t * lv_js_tab_create(lv_obj_t * parent);
void lv_js_tab_return(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_USE_JS_ENGINE */

#endif /* JS_TAB_H */
