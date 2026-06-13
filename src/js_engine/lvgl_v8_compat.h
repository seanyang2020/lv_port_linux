/**
 * @file lvgl_v8_compat.h
 *
 * LVGL v8 → v9 compatibility shims for lv_binding_js render code.
 *
 * The lv_binding_js native render was written against LVGL v8 APIs.
 * This header maps the most common v8 patterns to their v9 equivalents
 * so we can compile without rewriting every component.
 *
 * NOT a complete compatibility layer — only covers the APIs actually
 * used by the render components we build.
 */
#ifndef LVGL_V8_COMPAT_H
#define LVGL_V8_COMPAT_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Event access (v8 direct member → v9 accessor) ---- */

#ifndef lv_event_get_user_data
#define lv_event_get_user_data(e)  lv_event_get_user_data(e)
#endif

/* ---- Draw part check (v8 macro → removed in v9) ---- */

/* lv_obj_draw_part_check_type was removed in v9.
 * In v9 the draw task carries a type field; for now we stub it
 * to always return true (the callback must handle filtering itself). */
#define lv_obj_draw_part_check_type(dsc, class_p, type)  (true)

#ifndef LV_DRAW_PART_INIT
#define LV_DRAW_PART_INIT(dsc)  /* not needed in v9 */
#endif

/* ---- Private struct access ---- */
/* The render code accesses private LVGL struct members.
 * In v9 these are hidden behind opaque types.  We include
 * the internal headers to restore access. */

#ifdef LV_USE_PRIVATE_API
/* Already enabled — nothing to do */
#else
/* Try to enable private API access */
#define LV_USE_PRIVATE_API 1
#endif

#ifdef __cplusplus
}
#endif

#endif /* LVGL_V8_COMPAT_H */
