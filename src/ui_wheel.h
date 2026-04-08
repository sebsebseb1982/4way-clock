#pragma once

#include <stdint.h>

// Clear module-held LVGL object pointers before rebuilding the screen.
void ui_wheel_reset_refs();

// Build the wheel UI on the active LVGL screen.
void ui_wheel_build();

// Advance wheel animation and refresh the selected result.
void ui_wheel_tick(uint32_t now_ms);