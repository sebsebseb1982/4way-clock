#pragma once
#include <lvgl.h>

// Initialize XPT2046 touchscreen and calibration from SPIFFS.
// Registers the LVGL input device driver.
void touch_init();
