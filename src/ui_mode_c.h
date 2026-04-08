#pragma once

#include <stdint.h>

// Clear module-held LVGL object pointers before rebuilding the screen.
void ui_mode_c_reset_refs();

// Build the roulette UI for MODE_C on the active LVGL screen.
void ui_mode_c_build();

// Advance roulette animation and refresh the selected result.
void ui_mode_c_tick(uint32_t now_ms);