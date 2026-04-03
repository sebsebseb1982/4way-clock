#pragma once
#include <stdint.h>

// Clear module-held LVGL object pointers before rebuilding the screen.
void ui_countdown_reset_refs();

// Build the countdown UI on the active LVGL screen (landscape 320×240, rotation=3).
void ui_countdown_build();

// Update countdown display if MODE_COUNTDOWN is active.
void ui_countdown_update();

// Decrement countdown — call every loop() iteration regardless of active mode.
void countdown_tick(uint32_t now_ms);
