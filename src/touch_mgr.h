#pragma once
#include <lvgl.h>

// Initialize XPT2046 touchscreen and calibration from SPIFFS.
// Registers the LVGL input device driver.
void touch_init();

// Update the touchscreen controller rotation to match the current TFT rotation.
void touch_set_rotation(uint8_t rotation);
