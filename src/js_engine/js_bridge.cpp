/**
 * @file js_bridge.cpp
 *
 * JavaScript engine bridge — embeds QuickJS (via txiki.js) into LVGL.
 *
 * Architecture:
 *   js_engine_init()      → TJS_Initialize + TJS_NewRuntime + register lvgljs
 *   js_engine_run_script() → TJS_EvalScript() to load the JS bundle
 *   js_engine_tick()      → uv_run(UV_RUN_NOWAIT) — call from LVGL timer
 *   js_engine_cleanup()   → TJS_FreeRuntime + resource release
 *
 * Minimal lvgljs API:
 *   lvgljs.print(msg)         — log a message to LVGL log
 *   lvgljs.screenColor(hex)   — set screen background color
 *   lvgljs.label(text, x, y)  — create a label at position
 *
 * Full React/LVGL render (NativeRenderInit from lv_binding_js) is being
 * ported incrementally — see render/native/ for the source files.
 */

#include "js_bridge.h"

#if LV_USE_JS_ENGINE

#include "engine.hpp"      /* GetRuntime, TJSRuntime   */
#include "private.h"       /* txiki.js internals        */
#include "js_tab.h"        /* lv_js_tab_return          */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**********************
 *      DEFINES
 **********************/

#define MAX_WIDGETS      128   /* max number of tracked widgets  */
#define MAX_CALLBACKS    128   /* max number of JS callbacks     */

/**********************
 *  STATIC VARIABLES
 **********************/

static TJSRuntime  * g_rt         = NULL;
static uv_timer_t    g_render_timer;
static bool          g_running    = false;
static bool          g_inited     = false;

/* Widget / callback tracking */
static lv_obj_t    * g_widgets[MAX_WIDGETS];
static JSValue       g_callbacks[MAX_CALLBACKS];
static int           g_widget_count = 0;
static int           g_cb_count     = 0;

static JSContext   * g_js_ctx = NULL;  /* cached for event handlers */

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void render_timer_cb(uv_timer_t * handle);
static void register_minimal_api(JSContext * ctx);
static int  store_widget(lv_obj_t * obj);
static int  store_callback(JSContext * ctx, JSValue cb);
static void btn_event_cb(lv_event_t * e);

/* ---- Minimal lvgljs API functions ---- */

static JSValue js_print(JSContext * ctx, JSValue this_val,
                        int argc, JSValue * argv)
{
    (void)this_val;
    for (int i = 0; i < argc; i++) {
        const char * s = JS_ToCString(ctx, argv[i]);
        if (s) {
            LV_LOG_USER("[js] %s", s);
            JS_FreeCString(ctx, s);
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_screen_color(JSContext * ctx, JSValue this_val,
                                int argc, JSValue * argv)
{
    (void)this_val;
    uint32_t hex = 0x202020;
    if (argc > 0) JS_ToUint32(ctx, &hex, argv[0]);

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(hex), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    return JS_UNDEFINED;
}

static JSValue js_label(JSContext * ctx, JSValue this_val,
                         int argc, JSValue * argv)
{
    (void)this_val;
    const char * text = "";
    int x = 0, y = 0;

    if (argc > 0) text = JS_ToCString(ctx, argv[0]);
    if (argc > 1) JS_ToInt32(ctx, &x, argv[1]);
    if (argc > 2) JS_ToInt32(ctx, &y, argv[2]);

    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, text ? text : "");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);

    if (text) JS_FreeCString(ctx, text);
    return JS_NewInt32(ctx, store_widget(label));
}

static JSValue js_textbox(JSContext * ctx, JSValue this_val,
                           int argc, JSValue * argv)
{
    (void)this_val;
    int x = 0, y = 0, w = 200, h = 50;

    if (argc > 0) JS_ToInt32(ctx, &x, argv[0]);
    if (argc > 1) JS_ToInt32(ctx, &y, argv[1]);
    if (argc > 2) JS_ToInt32(ctx, &w, argv[2]);
    if (argc > 3) JS_ToInt32(ctx, &h, argv[3]);

    lv_obj_t * ta = lv_textarea_create(lv_screen_active());
    lv_obj_set_pos(ta, x, y);
    lv_obj_set_size(ta, w, h);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x303030), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0);

    return JS_NewInt32(ctx, store_widget(ta));
}

static JSValue js_set_text(JSContext * ctx, JSValue this_val,
                            int argc, JSValue * argv)
{
    (void)this_val;
    if (argc < 2) return JS_UNDEFINED;

    int id;
    const char * text;
    JS_ToInt32(ctx, &id, argv[0]);
    text = JS_ToCString(ctx, argv[1]);

    if (id >= 0 && id < g_widget_count && g_widgets[id]) {
        if (lv_obj_check_type(g_widgets[id], &lv_textarea_class))
            lv_textarea_set_text(g_widgets[id], text ? text : "");
        else if (lv_obj_check_type(g_widgets[id], &lv_label_class))
            lv_label_set_text(g_widgets[id], text ? text : "");
    }
    if (text) JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

/* Called from JS: lvgljs.exit() — returns to app list */
static JSValue js_exit(JSContext * ctx, JSValue this_val,
                        int argc, JSValue * argv)
{
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    lv_js_tab_return();
    return JS_UNDEFINED;
}

static JSValue js_btn(JSContext * ctx, JSValue this_val,
                       int argc, JSValue * argv)
{
    (void)this_val;
    const char * text = "Button";
    int x = 0, y = 0, w = 100, h = 40;

    if (argc > 0) text = JS_ToCString(ctx, argv[0]);
    if (argc > 1) JS_ToInt32(ctx, &x, argv[1]);
    if (argc > 2) JS_ToInt32(ctx, &y, argv[2]);
    if (argc > 3) JS_ToInt32(ctx, &w, argv[3]);
    if (argc > 4) JS_ToInt32(ctx, &h, argv[4]);

    lv_obj_t * btn = lv_btn_create(lv_screen_active());
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);

    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_center(lbl);

    int id = store_widget(btn);

    /* If a callback function was passed as 6th argument, attach it */
    if (argc > 5 && JS_IsFunction(ctx, argv[5])) {
        int cb_id = store_callback(ctx, argv[5]);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)cb_id);
    }

    if (text) JS_FreeCString(ctx, text);
    return JS_NewInt32(ctx, id);
}

/* ---- Widget / callback storage ---- */

static int store_widget(lv_obj_t * obj)
{
    if (g_widget_count >= MAX_WIDGETS) return -1;
    int id = g_widget_count;
    g_widgets[id] = obj;
    g_widget_count++;
    return id;
}

static int store_callback(JSContext * ctx, JSValue cb)
{
    if (g_cb_count >= MAX_CALLBACKS) return -1;
    int id = g_cb_count;
    g_callbacks[id] = JS_DupValue(ctx, cb);
    g_cb_count++;
    return id;
}

static void btn_event_cb(lv_event_t * e)
{
    int cb_id = (int)(intptr_t)lv_event_get_user_data(e);
    if (cb_id < 0 || cb_id >= g_cb_count) return;
    if (!g_js_ctx) return;

    JSValue ret = JS_Call(g_js_ctx, g_callbacks[cb_id],
                          JS_UNDEFINED, 0, NULL);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(g_js_ctx);
        const char * s = JS_ToCString(g_js_ctx, exc);
        LV_LOG_ERROR("[js] callback error: %s", s ? s : "?");
        if (s) JS_FreeCString(g_js_ctx, s);
        JS_FreeValue(g_js_ctx, exc);
    }
    JS_FreeValue(g_js_ctx, ret);
}

/**********************
 *  STATIC FUNCTIONS
 **********************/

static void render_timer_cb(uv_timer_t * handle)
{
    (void)handle;
    lv_timer_handler();
}

/**
 * Register a minimal lvgljs API on the global object.
 * This is a stopgap until the full NativeRender (React/LVGL)
 * components are ported to LVGL v9.
 */
static void register_minimal_api(JSContext * ctx)
{
    g_js_ctx = ctx;  /* cache for event callbacks */

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue lvgljs = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, lvgljs, "print",
                      JS_NewCFunction(ctx, js_print, "print", 1));
    JS_SetPropertyStr(ctx, lvgljs, "screenColor",
                      JS_NewCFunction(ctx, js_screen_color, "screenColor", 1));
    JS_SetPropertyStr(ctx, lvgljs, "label",
                      JS_NewCFunction(ctx, js_label, "label", 3));
    JS_SetPropertyStr(ctx, lvgljs, "textbox",
                      JS_NewCFunction(ctx, js_textbox, "textbox", 4));
    JS_SetPropertyStr(ctx, lvgljs, "setText",
                      JS_NewCFunction(ctx, js_set_text, "setText", 2));
    JS_SetPropertyStr(ctx, lvgljs, "btn",
                      JS_NewCFunction(ctx, js_btn, "btn", 6));
    JS_SetPropertyStr(ctx, lvgljs, "exit",
                      JS_NewCFunction(ctx, js_exit, "exit", 0));

    JS_SetPropertyStr(ctx, global, "lvgljs", lvgljs);
    JS_FreeValue(ctx, global);
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int js_engine_init(void)
{
    if (g_inited) return 0;

    /* ---- txiki.js bootstrap ---- */
    static char dummy_argv0[] = "lvglsim";
    char * argv[] = { dummy_argv0, NULL };
    TJS_Initialize(1, argv);

    g_rt = TJS_NewRuntime();
    if (!g_rt) {
        fprintf(stderr, "[js_engine] TJS_NewRuntime failed\n");
        return -1;
    }

    JSContext * ctx = g_rt->ctx;

    /* Register minimal lvgljs API */
    register_minimal_api(ctx);

    /* Start the 30-ms LVGL render timer */
    uv_timer_init(&g_rt->loop, &g_render_timer);
    g_render_timer.data = g_rt;
    if (uv_timer_start(&g_render_timer, render_timer_cb, 30, 30) != 0) {
        fprintf(stderr, "[js_engine] uv_timer_start failed\n");
        return -1;
    }

    g_inited = true;
    LV_LOG_USER("[js_engine] initialised (minimal API)");
    return 0;
}

int js_engine_run_script(const char * script_path)
{
    if (!g_rt) {
        fprintf(stderr, "[js_engine] not initialised\n");
        return -1;
    }

    JSContext * ctx = g_rt->ctx;

    /* Evaluate the JS file as a script */
    JSValue result = TJS_EvalScript(ctx, script_path);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(ctx);
        const char * exc_str = JS_ToCString(ctx, exc);
        fprintf(stderr, "[js_engine] JS exception in %s:\n%s\n",
                script_path, exc_str ? exc_str : "(unknown)");
        if (exc_str) JS_FreeCString(ctx, exc_str);
        JS_FreeValue(ctx, exc);
        JS_FreeValue(ctx, result);
        return -1;
    }

    JS_FreeValue(ctx, result);
    g_running = true;

    /* Process initial JS tasks */
    uv_run(&g_rt->loop, UV_RUN_NOWAIT);

    LV_LOG_USER("[js_engine] running: %s", script_path);
    return 0;
}

void js_engine_tick(void)
{
    if (!g_rt || !g_running) return;
    uv_run(&g_rt->loop, UV_RUN_NOWAIT);
}

void js_engine_cleanup(void)
{
    if (!g_rt) return;

    g_running = false;

    uv_timer_stop(&g_render_timer);
    uv_close((uv_handle_t *)&g_render_timer, NULL);
    uv_run(&g_rt->loop, UV_RUN_NOWAIT);

    /* Release stored JS callbacks */
    for (int i = 0; i < g_cb_count; i++) {
        JS_FreeValue(g_js_ctx, g_callbacks[i]);
    }

    TJS_FreeRuntime(g_rt);
    g_rt          = NULL;
    g_js_ctx      = NULL;
    g_inited      = false;
    g_widget_count = 0;
    g_cb_count    = 0;
    memset(g_widgets, 0, sizeof(g_widgets));
    memset(g_callbacks, 0, sizeof(g_callbacks));

    LV_LOG_USER("[js_engine] cleaned up");
}

int js_engine_is_running(void)
{
    return g_running ? 1 : 0;
}

TJSRuntime * GetRuntime(void)
{
    return g_rt;
}

#endif /* LV_USE_JS_ENGINE */
