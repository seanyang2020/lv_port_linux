/**
 * @file js_loader_tab.c — JS Loader tab wrapper
 *
 * Thin LVGL tab that delegates to libjsloader.so:
 *   1. jsloader_init()     — safety check + app scan
 *   2. jsloader_create_ui() — C-side app list + auto-start
 *
 * On failure, displays an inline error message so the tab
 * degrades gracefully rather than crashing the host app.
 */
#include "js_loader_tab.h"

#if LV_USE_JS_LOADER

#include "js_loader.h"
#include "safety_check.h"

lv_obj_t * js_loader_tab_create(lv_obj_t * parent)
{
    /* Phase 1: Initialise jsloader.
     * Runs the Layer-2 safety check (LVGL version, color depth, sanity). */
    int ret = jsloader_init(JSROOT_DIR);
    if (ret != 0) {
        LV_LOG_ERROR("[js_loader_tab] jsloader_init failed: %s",
                     safety_last_error());

        /* Display a user-visible error on the tab page */
        lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(parent, 20, 0);

        lv_obj_t * heading = lv_label_create(parent);
        lv_label_set_text(heading, "JS Loader");
        lv_obj_set_style_text_font(heading, &lv_font_montserrat_22, 0);

        lv_obj_t * err = lv_label_create(parent);
        lv_label_set_text_fmt(err,
            "Initialisation failed.\n\n%s\n\n"
            "The JS engine is unavailable.\n"
            "Check LVGL version compatibility.",
            safety_last_error());
        lv_obj_set_style_text_color(err, lv_color_hex(0xFF4444), 0);
        return parent;
    }

    /* Phase 2: Build the launcher UI (app list + auto-start dropdown).
     * This internally creates a C-side list UI.  In a future phase this
     * will be replaced by a JS launcher application. */
    return jsloader_create_ui(parent);
}

#endif /* LV_USE_JS_LOADER */
