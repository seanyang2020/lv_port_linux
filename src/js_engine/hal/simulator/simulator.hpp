/**
 * @file simulator.hpp
 *
 * Simulator HAL stub — lv_port_linux owns the display,
 * so the lv_binding_js simulator init is skipped.
 */
#pragma once

#include "lvgl/lvgl.h"

/* Stub variables referenced by window.cpp */
static void * buf;
static lv_coord_t hor_res;
static lv_coord_t ver_res;

static void (*flush_cb)(struct _lv_disp_drv_t *, const lv_area_t *, lv_color_t *) = NULL;

using read_cb = void (*)(struct _lv_indev_drv_t *, lv_indev_data_t *);
static read_cb read_cb1 = NULL;
static read_cb read_cb2 = NULL;
static read_cb read_cb3 = NULL;
