#pragma once
#include <lvgl.h>

// Initialize GT911 capacitive touch controller via I2C (SDA=19, SCL=45).
// The board shares GPIO38 with the display backlight, so touch init probes the
// controller without driving its reset pin.
// Registers the LVGL input device driver with rotation-aware coordinate mapping.
void touch_init();

// Update the rotation hint used by the LVGL touch read callback.
void touch_set_rotation(uint8_t rotation);
