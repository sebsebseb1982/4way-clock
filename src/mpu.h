#pragma once
#include <stdint.h>
#include "app_mode.h"

// Initialize MPU6050 on Wire bus (SDA=22, SCL=27).
// Returns true if device found and woken up successfully.
bool mpu_init();

// Call every ~50ms from loop(). Reads accelerometer, applies 100ms debounce.
// Returns true if the active mode has changed.
bool mpu_update(uint32_t now_ms);

// Get the current debounced mode.
AppMode mpu_current_mode();
