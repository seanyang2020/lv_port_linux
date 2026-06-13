/**
 * @file js_tab.c
 *
 * JS Apps tab — enumerates JS applications from a directory,
 * displays them as a list, and provides launch / return navigation.
 *
 * Directory layout convention:
 *   JS_APPS_DIR/
 *     hello_world/
 *       index.js          <-- JS bundle
 *     calculator/
 *       index.js
 *     ...
 *
 * Any subdirectory that contains an "index.js" file is treated
 * as a JS application.
 */

#include "js_tab.h"

#if LV_USE_JS_ENGINE

#include "js_bridge.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/*********************
 *      DEFINES
 *********************/

#ifndef JS_APPS_DIR
#define JS_APPS_DIR  "src/js_engine/apps"
#endif

#define MAX_APPS         32
#define MAX_APP_NAME_LEN 64
#define TICK_PERIOD_MS   30

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    char name[MAX_APP_NAME_LEN];
    char path[512];
} js_app_t;

typedef struct {
    lv_obj_t * list_screen;    /* app-list tab page             */
    lv_obj_t * list_obj;       /* lv_list widget                */
    lv_obj_t * js_screen;      /* screen active while JS runs   */
    lv_obj_t * prev_screen;    /* screen to restore on return   */
    lv_obj_t * back_btn;       /* floating back button          */
    lv_timer_t * tick_timer;   /* drives js_engine_tick()       */

    js_app_t   apps[MAX_APPS];
    int        app_count;
    int        current_app_idx;
} js_tab_ctx_t;

static js_tab_ctx_t g_ctx;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static int  scan_apps(void);
static void create_list_ui(void);
static void list_btn_event_cb(lv_event_t * e);
static void launch_app(int idx);
static void return_to_list(void);
static void back_btn_event_cb(lv_event_t * e);
static void tick_timer_cb(lv_timer_t * t);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Scan JS_APPS_DIR for subdirectories containing index.js.
 * Populates g_ctx.apps[] and returns the count.
 */
static int scan_apps(void)
{
    DIR * dir;
    struct dirent * entry;

    g_ctx.app_count = 0;
    memset(g_ctx.apps, 0, sizeof(g_ctx.apps));

    dir = opendir(JS_APPS_DIR);
    if (!dir) {
        LV_LOG_WARN("[js_tab] cannot open %s", JS_APPS_DIR);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL && g_ctx.app_count < MAX_APPS) {
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "..") == 0) continue;

        char index_path[512];
        snprintf(index_path, sizeof(index_path),
                 JS_APPS_DIR "/%s/index.js", entry->d_name);

        struct stat st;
        if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) {
            js_app_t * app = &g_ctx.apps[g_ctx.app_count];
            strncpy(app->name, entry->d_name, MAX_APP_NAME_LEN - 1);
            app->name[MAX_APP_NAME_LEN - 1] = '\0';
            snprintf(app->path, sizeof(app->path), "%s", index_path);
            g_ctx.app_count++;
        }
    }

    closedir(dir);
    LV_LOG_USER("[js_tab] found %d JS app(s)", g_ctx.app_count);
    return g_ctx.app_count;
}

/**
 * Create the app-list UI (called once when the tab is first shown).
 */
static void create_list_ui(void)
{
    /* The parent passed to lv_js_tab_create becomes list_screen */
    lv_obj_t * parent = g_ctx.list_screen;

    /* Use flex layout so content stays within the tab page */
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 10, 0);

    /* Heading */
    lv_obj_t * heading = lv_label_create(parent);
    lv_label_set_text_static(heading, "JavaScript Apps");
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_22, 0);

    /* Subtitle */
    lv_obj_t * subtitle = lv_label_create(parent);
    lv_label_set_text_fmt(subtitle, "Found %d application(s)", g_ctx.app_count);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);

    if (g_ctx.app_count == 0) {
        lv_obj_t * empty = lv_label_create(parent);
        lv_label_set_text_static(empty,
            "No JS apps found.\n\n"
            "Place JS bundles in:\n" JS_APPS_DIR "/<app_name>/index.js");
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    /* Scrollable list of apps — fills remaining space */
    g_ctx.list_obj = lv_list_create(parent);
    lv_obj_set_flex_grow(g_ctx.list_obj, 1);
    lv_obj_set_width(g_ctx.list_obj, lv_pct(100));

    for (int i = 0; i < g_ctx.app_count; i++) {
        lv_obj_t * btn = lv_list_add_btn(g_ctx.list_obj,
                                         NULL,    /* no icon */
                                         g_ctx.apps[i].name);
        lv_obj_add_event_cb(btn, list_btn_event_cb,
                            LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

/**
 * Callback when a list item is clicked — launch the JS app.
 */
static void list_btn_event_cb(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    launch_app(idx);
}

/**
 * Create a new screen, execute the JS bundle, and show a Back button.
 */
static void launch_app(int idx)
{
    if (idx < 0 || idx >= g_ctx.app_count) return;

    g_ctx.current_app_idx = idx;
    js_app_t * app = &g_ctx.apps[idx];

    LV_LOG_USER("[js_tab] launching: %s", app->name);

    /* Save the current screen so we can restore it on return */
    g_ctx.prev_screen = lv_screen_active();

    /* ---- Immersive full-screen: no title bar, just a minimal
     *     floating back button in the top-right corner ---- */

    g_ctx.js_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_ctx.js_screen,
                              lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(g_ctx.js_screen, LV_OPA_COVER, 0);

    /* Floating back button (top-right, small, semi-transparent) */
    g_ctx.back_btn = lv_btn_create(g_ctx.js_screen);
    lv_obj_set_size(g_ctx.back_btn, 32, 32);
    lv_obj_align(g_ctx.back_btn, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_set_style_bg_opa(g_ctx.back_btn, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(g_ctx.back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_radius(g_ctx.back_btn, 16, 0);
    lv_obj_add_event_cb(g_ctx.back_btn, back_btn_event_cb,
                        LV_EVENT_CLICKED, NULL);

    lv_obj_t * back_label = lv_label_create(g_ctx.back_btn);
    lv_label_set_text_static(back_label, LV_SYMBOL_CLOSE);
    lv_obj_center(back_label);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);

    /* Load screen BEFORE running JS so lv_screen_active() is correct */
    lv_screen_load(g_ctx.js_screen);

    /* Initialize engine and run the script */
    js_engine_init();
    js_engine_run_script(app->path);

    /* Start the tick timer to drive libuv */
    g_ctx.tick_timer = lv_timer_create(tick_timer_cb,
                                        TICK_PERIOD_MS, NULL);
}

/**
 * LVGL timer callback — drives js_engine_tick().
 */
static void tick_timer_cb(lv_timer_t * t)
{
    (void)t;
    js_engine_tick();
}

/**
 * Back button callback — return to app list.
 */
static void back_btn_event_cb(lv_event_t * e)
{
    (void)e;
    return_to_list();
}

/**
 * Return from the JS app to the widgets demo screen.
 */
static void return_to_list(void)
{
    LV_LOG_USER("[js_tab] returning to app list");

    /* Stop JS engine first (deletes libuv timer, frees runtime) */
    if (g_ctx.tick_timer) {
        lv_timer_delete(g_ctx.tick_timer);
        g_ctx.tick_timer = NULL;
    }
    js_engine_cleanup();

    /* Delete the JS screen (destroys all JS-created widgets) */
    if (g_ctx.js_screen) {
        lv_obj_delete(g_ctx.js_screen);
        g_ctx.js_screen = NULL;
    }

    g_ctx.back_btn = NULL;

    /* Restore the original screen (widgets demo) */
    if (g_ctx.prev_screen) {
        lv_screen_load(g_ctx.prev_screen);
        g_ctx.prev_screen = NULL;
    }
}

/**********************
 *   GLOBAL FUNCTION
 **********************/

lv_obj_t * lv_js_tab_create(lv_obj_t * parent)
{
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.list_screen = parent;

    scan_apps();
    create_list_ui();

    return parent;
}

/**
 * Called from JS (lvgljs.exit()) or programmatically to return
 * to the app list while a JS app is running.
 */
void lv_js_tab_return(void)
{
    if (!g_ctx.js_screen) return;  /* no JS app running */
    return_to_list();
}

#endif /* LV_USE_JS_ENGINE */
