#pragma once
#include <time.h>

// Build the clock UI on the active LVGL screen (landscape 320×240, rotation=1).
// canvas_buf is allocated on first call and preserved across mode switches.
void ui_clock_build();

// Update clock hands and info panel. Call every second when MODE_CLOCK is active.
void ui_clock_update(struct tm *t, bool synced);

// Force-refresh the WiFi/NTP status labels (call after WiFi connect in setup).
void ui_clock_refresh_status(bool wifi_ok, const char *ip);
