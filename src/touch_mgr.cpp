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
#define GT911_POINT1_REG  0x814F  // 8 bytes: track_id, x_lo, x_hi, y_lo, y_hi, ...
#define GT911_X_MAX_REG   0x8048  // little-endian
#define GT911_Y_MAX_REG   0x804A  // little-endian

#define TOUCH_MAX_X  479
#define TOUCH_MAX_Y  479

// Flag set to true only if GT911 is reachable at init; guards touch_read_cb.
static bool s_gt911_found = false;
static uint8_t s_gt911_addr = GT911_ADDR1;
static uint16_t s_gt911_max_x = TOUCH_MAX_X;
static uint16_t s_gt911_max_y = TOUCH_MAX_Y;

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

static uint16_t gt911_read_u16le(uint16_t reg) {
    uint16_t lo = gt911_read_byte(reg);
    uint16_t hi = gt911_read_byte(reg + 1);
    return (uint16_t)(lo | (hi << 8));
}

static size_t gt911_read_block(uint16_t reg, uint8_t *buf, size_t len) {
    Wire.beginTransmission(s_gt911_addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission(false) != 0) {
        return 0;
    }

    size_t count = Wire.requestFrom((uint8_t)s_gt911_addr, (uint8_t)len);
    for (size_t i = 0; i < len; i++) {
        if (!Wire.available()) {
            return i;
        }
        buf[i] = Wire.read();
    }
    return count;
}

static int16_t gt911_scale_axis(uint16_t coord, uint16_t sensor_max, int16_t panel_max) {
    if (sensor_max == 0) {
        return coord <= (uint16_t)panel_max ? (int16_t)coord : -1;
    }
    if (coord > sensor_max) {
        return -1;
    }

    uint32_t scaled = ((uint32_t)coord * (uint32_t)panel_max) + (sensor_max / 2u);
    scaled /= sensor_max;
    return scaled <= (uint32_t)panel_max ? (int16_t)scaled : -1;
}

static bool gt911_map_sensor_to_panel(uint16_t raw_x, uint16_t raw_y,
                                      int16_t &panel_x, int16_t &panel_y) {
    int16_t sensor_x = gt911_scale_axis(raw_x, s_gt911_max_x, TOUCH_MAX_X);
    int16_t sensor_y = gt911_scale_axis(raw_y, s_gt911_max_y, TOUCH_MAX_Y);
    if (sensor_x < 0 || sensor_y < 0) {
        return false;
    }

    // The touch sensor is mounted 90 degrees from the LCD and one axis is
    // mirrored relative to the panel coordinates.
    panel_x = sensor_y;
    panel_y = TOUCH_MAX_Y - sensor_x;
    return true;
}

// Returns true and fills raw GT911 coordinates plus normalized panel coordinates.
static bool gt911_read_touch(uint16_t &raw_x, uint16_t &raw_y,
                             int16_t &panel_x, int16_t &panel_y) {
    uint8_t status = gt911_read_byte(GT911_STATUS_REG);
    uint8_t n = status & 0x0F;
    bool ready  = (status & 0x80) != 0;

    if (!ready || n == 0) {
        if (ready) gt911_write_reg(GT911_STATUS_REG, 0);   // clear stale flag
        return false;
    }

    uint8_t buf[8] = {};
    size_t count = gt911_read_block(GT911_POINT1_REG, buf, sizeof(buf));

    gt911_write_reg(GT911_STATUS_REG, 0);   // clear buffer-ready flag

    if (count < 5) {
        return false;
    }

    // buf[0]=track_id, buf[1]=x_lo, buf[2]=x_hi, buf[3]=y_lo, buf[4]=y_hi
    raw_x = (uint16_t)(buf[1] | ((uint16_t)buf[2] << 8));
    raw_y = (uint16_t)(buf[3] | ((uint16_t)buf[4] << 8));
    return gt911_map_sensor_to_panel(raw_x, raw_y, panel_x, panel_y);
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

static void touch_calc_lvgl_point(lv_disp_t *disp, int16_t in_x, int16_t in_y,
                                  int16_t &out_x, int16_t &out_y) {
    lv_disp_rot_t rot = disp ? lv_disp_get_rotation(disp) : LV_DISP_ROT_NONE;
    lv_coord_t hor_res = disp ? lv_disp_get_hor_res(disp) : TOUCH_MAX_X + 1;
    lv_coord_t ver_res = disp ? lv_disp_get_ver_res(disp) : TOUCH_MAX_Y + 1;

    out_x = in_x;
    out_y = in_y;

    if (rot == LV_DISP_ROT_180 || rot == LV_DISP_ROT_270) {
        out_x = (int16_t)(hor_res - out_x - 1);
        out_y = (int16_t)(ver_res - out_y - 1);
    }
    if (rot == LV_DISP_ROT_90 || rot == LV_DISP_ROT_270) {
        int16_t tmp = out_y;
        out_y = out_x;
        out_x = (int16_t)(ver_res - tmp - 1);
    }
}

void touch_set_rotation(uint8_t rotation) {
    // GT911 outputs absolute panel coordinates. LVGL rotates input points
    // internally based on disp_drv.rotated, so no extra transform is applied
    // in the touch driver.
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

    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    int16_t panel_x = 0;
    int16_t panel_y = 0;
    bool pressed = gt911_read_touch(raw_x, raw_y, panel_x, panel_y);

    if (!pressed) {
        if (s_prev_pressed) {
            Serial.printf("[Touch] RELEASE mode=%s lvgl_rot=%s\n",
                          mode_name(g_current_mode), lvgl_rot_name(rot));
        }
        s_prev_pressed = false;
        data->state    = LV_INDEV_STATE_REL;
        return;
    }

    int16_t lx = panel_x;
    int16_t ly = panel_y;
    int16_t logical_x = lx;
    int16_t logical_y = ly;
    touch_calc_lvgl_point(disp, lx, ly, logical_x, logical_y);

    if (lx < 0 || lx > TOUCH_MAX_X || ly < 0 || ly > TOUCH_MAX_Y) {
        if (s_prev_pressed) {
            Serial.printf("[Touch] RELEASE invalid raw=(%u,%u) panel=(%d,%d) mapped=(%d,%d)\n",
                          (unsigned)raw_x, (unsigned)raw_y, panel_x, panel_y, lx, ly);
        }
        s_prev_pressed = false;
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    if (!s_prev_pressed || now - s_last_press_log >= 250) {
        Serial.printf("[Touch] PRESS mode=%s lvgl_rot=%s raw=(%u,%u) panel=(%d,%d) lvgl_in=(%d,%d) lvgl_out=(%d,%d)\n",
                      mode_name(g_current_mode), lvgl_rot_name(rot),
                      (unsigned)raw_x, (unsigned)raw_y, panel_x, panel_y,
                      lx, ly, logical_x, logical_y);
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
        uint16_t sensor_x = gt911_read_u16le(GT911_X_MAX_REG);
        uint16_t sensor_y = gt911_read_u16le(GT911_Y_MAX_REG);
        if (sensor_x != 0) s_gt911_max_x = sensor_x - 1;
        if (sensor_y != 0) s_gt911_max_y = sensor_y - 1;
        Serial.printf("[Touch] GT911 OK SDA=%d SCL=%d INT=%d addr=0x%02X\n",
                      GT911_SDA, GT911_SCL, GT911_INT, s_gt911_addr);
        Serial.printf("[Touch] Sensor range x=0..%u y=0..%u\n",
                      (unsigned)s_gt911_max_x, (unsigned)s_gt911_max_y);
    }

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
    Serial.println("[Touch] Driver LVGL enregistre");
}
