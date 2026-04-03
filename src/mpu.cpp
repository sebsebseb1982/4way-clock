#include "mpu.h"
#include <Wire.h>
#include <Arduino.h>

#define MPU_ADDR  0x68
#define MPU_SDA   22
#define MPU_SCL   27

// Debounce threshold
#define ORIENT_THRESHOLD_G  0.7f
#define DEBOUNCE_MS         100

static AppMode s_debounced_mode    = MODE_CLOCK;
static AppMode s_candidate_mode    = MODE_CLOCK;
static uint32_t s_candidate_since  = 0;

// ─────────────────────────────────────────────────────────────────────────────
bool mpu_init() {
    Wire.begin(MPU_SDA, MPU_SCL);

    // Wake up MPU6050 (clear sleep bit in PWR_MGMT_1)
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);  // PWR_MGMT_1 register
    Wire.write(0x00);  // wake up
    uint8_t err = Wire.endTransmission(true);

    if (err != 0) {
        Serial.printf("[MPU] ERREUR: MPU6050 non trouve sur 0x%02X (err=%d)\n", MPU_ADDR, err);
        return false;
    }

    // Set accelerometer range to ±2g (ACCEL_CONFIG register 0x1C = 0x00)
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1C);
    Wire.write(0x00);
    Wire.endTransmission(true);

    Serial.printf("[MPU] MPU6050 initialise sur SDA=%d SCL=%d\n", MPU_SDA, MPU_SCL);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
static bool mpu_read_accel(float &ax, float &ay) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);  // ACCEL_XOUT_H
    if (Wire.endTransmission(false) != 0) return false;

    Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)4, (uint8_t)true);
    if (Wire.available() < 4) return false;

    int16_t raw_x = (int16_t)(Wire.read() << 8 | Wire.read());
    int16_t raw_y = (int16_t)(Wire.read() << 8 | Wire.read());

    ax = raw_x / 16384.0f;   // ±2g range: 16384 LSB/g
    ay = raw_y / 16384.0f;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
static AppMode detect_orientation(float ax, float ay) {
    float abs_ax = fabsf(ax);
    float abs_ay = fabsf(ay);
    float dominant = (abs_ax > abs_ay) ? abs_ax : abs_ay;

    if (dominant < ORIENT_THRESHOLD_G) return s_debounced_mode; // AMBIGUOUS: keep current

    if (abs_ax >= abs_ay) {
        return (ax > 0) ? MODE_CLOCK : MODE_COUNTDOWN;
    } else {
        return (ay > 0) ? MODE_C : MODE_D;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
bool mpu_update(uint32_t now_ms) {
    float ax, ay;
    if (!mpu_read_accel(ax, ay)) return false;

    AppMode raw = detect_orientation(ax, ay);

    if (raw != s_candidate_mode) {
        s_candidate_mode  = raw;
        s_candidate_since = now_ms;
        return false;
    }

    if ((now_ms - s_candidate_since) >= DEBOUNCE_MS && s_candidate_mode != s_debounced_mode) {
        s_debounced_mode = s_candidate_mode;
        return true;  // mode changed
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
AppMode mpu_current_mode() {
    return s_debounced_mode;
}
