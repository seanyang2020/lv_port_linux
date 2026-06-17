/**
 * @file js_loader_tab.h — JS Loader tab wrapper
 *
 * Thin LVGL tab that delegates to libjsloader.so.
 * Compile-time gated on LV_USE_JS_LOADER.
 */
#ifndef JS_LOADER_TAB_H
#define JS_LOADER_TAB_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the JS Loader tab content on the given page.
 *
 * Calls jsloader_init(JSROOT_DIR) then jsloader_create_ui(page).
 * JSROOT_DIR is set by CMake at compile time.
 *
 * @param parent  LVGL page object (from lv_tabview_add_tab)
 * @return parent on success, or parent with error label on failure.
 */
lv_obj_t * js_loader_tab_create(lv_obj_t * parent);

#ifdef __cplusplus
}
#endif

#endif /* JS_LOADER_TAB_H */
