#include "touch_mgr.h"
#include "app_mode.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#define TOUCH_CS    33
#define TOUCH_IRQ   36
#define TOUCH_CLK   25
#define TOUCH_MISO  39
#define TOUCH_MOSI  32

#define TOUCH_X_MIN 300
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 300
#define TOUCH_Y_MAX 3800

static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen touchscreen(TOUCH_CS, TOUCH_IRQ);

static const char *mode_name(AppMode mode) {
    switch (mode) {
        case MODE_CLOCK: return "CLOCK";
        case MODE_COUNTDOWN: return "COUNTDOWN";
        case MODE_C: return "C";
        case MODE_D: return "D";
        default: return "?";
    }
}

static const char *lvgl_rot_name(lv_disp_rot_t rot) {
    switch (rot) {
        case LV_DISP_ROT_NONE: return "0";
        case LV_DISP_ROT_90: return "90";
        case LV_DISP_ROT_180: return "180";
        case LV_DISP_ROT_270: return "270";
        default: return "?";
    }
}

static void touch_map_point(const TS_Point &p, int16_t &sx, int16_t &sy) {
    bool landscape = (g_active_rotation % 2 == 1);
    int16_t max_x = landscape ? 319 : 239;
    int16_t max_y = landscape ? 239 : 319;

    sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, max_x);
    sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, max_y);
    sx = constrain(sx, 0, max_x);
    sy = constrain(sy, 0, max_y);
}

void touch_set_rotation(uint8_t rotation) {
    touchscreen.setRotation(rotation % 4);
    Serial.printf("[Touch] Rotation set -> hw_rot=%u\n", (unsigned)(rotation % 4));
}

// ─────────────────────────────────────────────────────────────────────────────
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    static bool     s_prev_pressed    = false;
    static uint32_t s_last_press_log  = 0;
    static uint32_t s_last_reject_log = 0;

    uint32_t now = millis();
    lv_disp_t *disp = lv_disp_get_default();
    lv_disp_rot_t lvgl_rot = disp ? lv_disp_get_rotation(disp) : LV_DISP_ROT_NONE;
    bool pressed = touchscreen.touched();

    if (!pressed) {
        if (s_prev_pressed) {
            Serial.printf(
                "[Touch] RELEASE mode=%s hw_rot=%u lvgl_rot=%s\n",
                mode_name(g_current_mode),
                (unsigned)g_active_rotation,
                lvgl_rot_name(lvgl_rot));
        }
        s_prev_pressed = false;
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    TS_Point p = touchscreen.getPoint();
    int16_t lx, ly;
    touch_map_point(p, lx, ly);

    // Bounds check against current LVGL resolution
    int16_t max_x = (g_active_rotation % 2 == 0) ? 239 : 319;
    int16_t max_y = (g_active_rotation % 2 == 0) ? 319 : 239;

    if (lx >= 0 && lx <= max_x && ly >= 0 && ly <= max_y) {
        if (!s_prev_pressed || now - s_last_press_log >= 250) {
            Serial.printf(
                "[Touch] PRESS mode=%s hw_rot=%u lvgl_rot=%s raw=(%d,%d,%d) mapped=(%d,%d) lim=(%d,%d)\n",
                mode_name(g_current_mode),
                (unsigned)g_active_rotation,
                lvgl_rot_name(lvgl_rot),
                p.x,
                p.y,
                p.z,
                lx,
                ly,
                max_x,
                max_y);
            s_last_press_log = now;
        }
        s_prev_pressed = true;
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = lx;
        data->point.y = ly;
    } else {
        if (now - s_last_reject_log >= 250) {
            Serial.printf(
                "[Touch] REJECT mode=%s hw_rot=%u lvgl_rot=%s raw=(%d,%d,%d) mapped=(%d,%d) lim=(%d,%d)\n",
                mode_name(g_current_mode),
                (unsigned)g_active_rotation,
                lvgl_rot_name(lvgl_rot),
                p.x,
                p.y,
                p.z,
                lx,
                ly,
                max_x,
                max_y);
            s_last_reject_log = now;
        }
        s_prev_pressed = false;
        data->state = LV_INDEV_STATE_REL;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void touch_init() {
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touchscreen.begin(touchSPI);
    touch_set_rotation(g_active_rotation);
    Serial.printf("[Touch] XPT2046 init cs=%d irq=%d clk=%d miso=%d mosi=%d\n",
                  TOUCH_CS, TOUCH_IRQ, TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI);
    Serial.printf("[Touch] Calibration x=[%d..%d] y=[%d..%d]\n",
                  TOUCH_X_MIN, TOUCH_X_MAX, TOUCH_Y_MIN, TOUCH_Y_MAX);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
    Serial.println("[Touch] Driver LVGL enregistre");
}
