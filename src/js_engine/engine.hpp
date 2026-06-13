/**
 * @file engine.hpp
 *
 * Bridge header — provides the GetRuntime() function that render
 * components use to access the TJSRuntime.  Mirrors the original
 * lv_binding_js/src/engine/engine.hpp.
 */
#pragma once

extern "C" {
    #include "private.h"
}

TJSRuntime* GetRuntime(void);
