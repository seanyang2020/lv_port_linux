/**
 * @file hal.hpp
 *
 * Hardware abstraction layer — stub for embedded mode.
 * lv_port_linux already initializes the display and input,
 * so hal_init() is a no-op.
 */
#pragma once

static inline void hal_init(void) { }
