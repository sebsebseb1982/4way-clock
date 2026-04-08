#include "touch_mgr.h"
#include "app_mode.h"
#include <Arduino.h>
#include <Wire.h>

// ── GT911 capacitive touch — ESP32-4848S040 pinout ───────────────────────────
#define GT911_SDA   19
#define GT911_SCL   45

// The exact INT pin is board/firmware dependent. Polling over I2C is enough for
// LVGL, so do not reconfigure a GPIO here unless it has been electrically
// verified. On this board GPIO18 is the RGB DE signal; touching it blanks the
// panel.
#define GT911_INT   (-1)

#define GT911_ADDR1 0x14
#define GT911_ADDR2 0x5D

// GT911 register addresses (16-bit, MSB first)
#define GT911_STATUS_REG  0x814E  // bits[7]=buffer_ready, bits[3:0]=num_touches
#define GT911_POINT1_REG  0x8150  // 8 bytes: track_id, x_lo, x_hi, y_lo, y_hi, ...

#define TOUCH_MAX_X  479
#define TOUCH_MAX_Y  479

// Flag set to true only if GT911 is reachable at init; guards touch_read_cb.
static bool s_gt911_found = false;
static uint8_t s_gt911_addr = GT911_ADDR1;

static bool gt911_select_addr(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// ── Helpers ──────────────────────────────────────────────────────────────────
static void gt911_write_reg(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(s_gt911_addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t gt911_read_byte(uint16_t reg) {
    Wire.beginTransmission(s_gt911_addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)s_gt911_addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
}

// Returns true and fills (x, y) in raw screen coordinates [0..479] if touched.
static bool gt911_read_touch(int16_t &x, int16_t &y) {
    uint8_t status = gt911_read_byte(GT911_STATUS_REG);
    uint8_t n = status & 0x0F;
    bool ready  = (status & 0x80) != 0;

    if (!ready || n == 0) {
        if (ready) gt911_write_reg(GT911_STATUS_REG, 0);   // clear stale flag
        return false;
    }

    // Read first touch point: track_id(1) x_lo(1) x_hi(1) y_lo(1) y_hi(1) ...
    Wire.beginTransmission(s_gt911_addr);
    Wire.write(GT911_POINT1_REG >> 8);
    Wire.write(GT911_POINT1_REG & 0xFF);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)s_gt911_addr, (uint8_t)5);

    uint8_t buf[5] = {};
    for (int i = 0; i < 5 && Wire.available(); i++) buf[i] = Wire.read();

    gt911_write_reg(GT911_STATUS_REG, 0);   // clear buffer-ready flag

    // buf[0]=track_id, buf[1]=x_lo, buf[2]=x_hi, buf[3]=y_lo, buf[4]=y_hi
    x = (int16_t)(buf[1] | ((uint16_t)buf[2] << 8));
    y = (int16_t)(buf[3] | ((uint16_t)buf[4] << 8));
    x = constrain(x, 0, TOUCH_MAX_X);
    y = constrain(y, 0, TOUCH_MAX_Y);
    return true;
}

// Apply LVGL software-rotation coordinate transform for a 480×480 display.
static void touch_apply_rotation(int16_t raw_x, int16_t raw_y,
                                  int16_t &lx,   int16_t &ly) {
    lv_disp_rot_t rot = LV_DISP_ROT_NONE;
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) rot = lv_disp_get_rotation(disp);

    switch (rot) {
        case LV_DISP_ROT_90:
            lx = raw_y;
            ly = TOUCH_MAX_X - raw_x;
            break;
        case LV_DISP_ROT_180:
            lx = TOUCH_MAX_X - raw_x;
            ly = TOUCH_MAX_Y - raw_y;
            break;
        case LV_DISP_ROT_270:
            lx = TOUCH_MAX_Y - raw_y;
            ly = raw_x;
            break;
        default:   // LV_DISP_ROT_NONE
            lx = raw_x;
            ly = raw_y;
            break;
    }
}

static const char *mode_name(AppMode mode) {
    switch (mode) {
        case MODE_CLOCK:     return "CLOCK";
        case MODE_COUNTDOWN: return "COUNTDOWN";
        case MODE_C:         return "C";
        case MODE_D:         return "D";
        default:             return "?";
    }
}

static const char *lvgl_rot_name(lv_disp_rot_t rot) {
    switch (rot) {
        case LV_DISP_ROT_NONE: return "0";
        case LV_DISP_ROT_90:   return "90";
        case LV_DISP_ROT_180:  return "180";
        case LV_DISP_ROT_270:  return "270";
        default:               return "?";
    }
}

void touch_set_rotation(uint8_t rotation) {
    // GT911 outputs absolute screen coordinates; rotation is handled in
    // touch_apply_rotation() inside the LVGL read callback.
    Serial.printf("[Touch] Rotation hint set -> mode=%u\n", (unsigned)rotation);
}

// ─────────────────────────────────────────────────────────────────────────────
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    // GT911 absent : ne pas tenter de lire via I2C pour éviter les erreurs Wire
    if (!s_gt911_found) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    static bool     s_prev_pressed   = false;
    static uint32_t s_last_press_log = 0;

    uint32_t now = millis();
    lv_disp_t *disp     = lv_disp_get_default();
    lv_disp_rot_t rot   = disp ? lv_disp_get_rotation(disp) : LV_DISP_ROT_NONE;

    int16_t raw_x, raw_y;
    bool pressed = gt911_read_touch(raw_x, raw_y);

    if (!pressed) {
        if (s_prev_pressed) {
            Serial.printf("[Touch] RELEASE mode=%s lvgl_rot=%s\n",
                          mode_name(g_current_mode), lvgl_rot_name(rot));
        }
        s_prev_pressed = false;
        data->state    = LV_INDEV_STATE_REL;
        return;
    }

    int16_t lx, ly;
    touch_apply_rotation(raw_x, raw_y, lx, ly);

    if (!s_prev_pressed || now - s_last_press_log >= 250) {
        Serial.printf("[Touch] PRESS mode=%s lvgl_rot=%s raw=(%d,%d) mapped=(%d,%d)\n",
                      mode_name(g_current_mode), lvgl_rot_name(rot),
                      raw_x, raw_y, lx, ly);
        s_last_press_log = now;
    }
    s_prev_pressed = true;
    data->state    = LV_INDEV_STATE_PR;
    data->point.x  = lx;
    data->point.y  = ly;
}

// ─────────────────────────────────────────────────────────────────────────────
void touch_init() {
    // GPIO38 drives the display backlight on this board, so the touch reset line
    // cannot be toggled safely here. Probe both standard GT911 addresses instead.
    // INT is intentionally left untouched because misconfiguring it can steal a
    // display signal on ESP32-4848S040 variants.
    if (GT911_INT >= 0) {
        pinMode(GT911_INT, INPUT);
    } else {
        Serial.println("[Touch] INT non configure: mode polling I2C uniquement");
    }

    Wire.begin(GT911_SDA, GT911_SCL);
    Wire.setClock(400000);

    if (gt911_select_addr(GT911_ADDR1)) {
        s_gt911_addr = GT911_ADDR1;
        s_gt911_found = true;
    } else if (gt911_select_addr(GT911_ADDR2)) {
        s_gt911_addr = GT911_ADDR2;
        s_gt911_found = true;
    } else {
        s_gt911_found = false;
        Serial.printf("[Touch] ERREUR: GT911 non trouve sur SDA=%d SCL=%d adrs=0x%02X/0x%02X\n",
                      GT911_SDA, GT911_SCL, GT911_ADDR1, GT911_ADDR2);
    }

    if (s_gt911_found) {
        Serial.printf("[Touch] GT911 OK SDA=%d SCL=%d INT=%d addr=0x%02X\n",
                      GT911_SDA, GT911_SCL, GT911_INT, s_gt911_addr);
    }

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
    Serial.println("[Touch] Driver LVGL enregistre");
}
