#include "touch_mgr.h"
#include "app_mode.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <XPT2046_Bitbang.h>

#define MOSI_PIN 32
#define MISO_PIN 39
#define CLK_PIN  25
#define CS_PIN   33

static XPT2046_Bitbang touchscreen(MOSI_PIN, MISO_PIN, CLK_PIN, CS_PIN);

// ─────────────────────────────────────────────────────────────────────────────
// Coordinate remapping
//
// The XPT2046 calibration is performed in portrait hardware space (240×320):
//   portrait_x : 0 – 239
//   portrait_y : 0 – 319
//
// We remap to the current LVGL logical space based on g_active_rotation.
// ─────────────────────────────────────────────────────────────────────────────
static void remap_touch(int16_t px, int16_t py,
                         int16_t &lx, int16_t &ly) {
    switch (g_active_rotation) {
        case 0:  // portrait 240×320
            lx = px;
            ly = py;
            break;
        case 1:  // landscape 320×240 (USB bas)
            lx = py;
            ly = 239 - px;
            break;
        case 2:  // portrait inversé 240×320
            lx = 239 - px;
            ly = 319 - py;
            break;
        case 3:  // landscape inversé 320×240 (USB haut)
            lx = 319 - py;
            ly = px;
            break;
        default:
            lx = px;
            ly = py;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    Point tp = touchscreen.getTouch();

    // XPT2046_Bitbang returns x/y; swap to get portrait space
    int16_t px = (int16_t)tp.y;
    int16_t py = (int16_t)tp.x;

    int16_t lx, ly;
    remap_touch(px, py, lx, ly);

    // Bounds check against current LVGL resolution
    int16_t max_x = (g_active_rotation % 2 == 0) ? 239 : 319;
    int16_t max_y = (g_active_rotation % 2 == 0) ? 319 : 239;

    if (lx >= 0 && lx <= max_x && ly >= 0 && ly <= max_y) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = lx;
        data->point.y = ly;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void touch_init() {
    if (!SPIFFS.begin(true)) {
        Serial.println("[Touch] SPIFFS mount failed");
    }

    touchscreen.begin();

    if (!touchscreen.loadCalibration()) {
        Serial.println("[Touch] Pas de calibration SPIFFS - lancement calibration...");
        delay(2000);
        touchscreen.calibrate();
        touchscreen.saveCalibration();
    } else {
        Serial.println("[Touch] Calibration chargee depuis SPIFFS");
    }

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
    Serial.println("[Touch] Driver LVGL enregistre");
}
