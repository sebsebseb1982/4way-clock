#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "wifi_ntp.h"

// ---- LVGL display driver -------------------------------------------------------
static TFT_eSPI tft;

// Partial framebuffer: 1/10th of screen height to save SRAM
static const uint16_t LV_BUF_HEIGHT = 320 / 10;
static lv_color_t lv_buf1[240 * LV_BUF_HEIGHT];
static lv_disp_draw_buf_t lv_draw_buf;
static lv_disp_drv_t      lv_disp_drv;

static void tft_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(reinterpret_cast<uint16_t *>(px_map), w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

// ---- WiFi / NTP retry state ---------------------------------------------------
static bool     s_wifiOk    = false;
static bool     s_ntpOk     = false;
static uint32_t s_lastRetry = 0;
static const uint32_t RETRY_INTERVAL_MS = 30000UL;

// ---- Placeholders (filled in by groups 3-6) -----------------------------------
// void clockFaceCreate(lv_obj_t *parent);
// void clockHandsCreate(lv_obj_t *parent);
// void clockHandsUpdate(const struct tm *t);
// void digitalDisplayCreate(lv_obj_t *parent);
// void digitalDisplayUpdate(const struct tm *t);

void setup() {
    Serial.begin(115200);

    // Backlight on
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // TFT init
    tft.init();
    tft.setRotation(1); // Landscape 320x240

    // LVGL init
    lv_init();
    lv_disp_draw_buf_init(&lv_draw_buf, lv_buf1, nullptr, 240 * LV_BUF_HEIGHT);
    lv_disp_drv_init(&lv_disp_drv);
    lv_disp_drv.hor_res  = 320;
    lv_disp_drv.ver_res  = 240;
    lv_disp_drv.flush_cb = tft_flush_cb;
    lv_disp_drv.draw_buf = &lv_draw_buf;
    lv_disp_drv_register(&lv_disp_drv);

    // Black background
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);

    // WiFi + NTP
    s_wifiOk = wifiConnect();
    if (s_wifiOk) {
        s_ntpOk = ntpInit();
    }

    // TODO (group 3-6): create clock face, hands, digital label
    // clockFaceCreate(lv_scr_act());
    // clockHandsCreate(lv_scr_act());
    // digitalDisplayCreate(lv_scr_act());
}

void loop() {
    static uint32_t last_tick = 0;
    uint32_t now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    lv_timer_handler();

    // Update clock every 100 ms
    static uint32_t last_update = 0;
    if (now - last_update >= 100) {
        last_update = now;
        struct tm t;
        bool synced = getCurrentTime(&t);

        // TODO (group 4-5): update hands and digital display
        // clockHandsUpdate(&t);
        // digitalDisplayUpdate(synced ? &t : nullptr);
        (void)synced;
    }

    // Retry WiFi + NTP every 30 s if not connected
    if ((!s_wifiOk || !s_ntpOk) && (now - s_lastRetry >= RETRY_INTERVAL_MS)) {
        s_lastRetry = now;
        if (!s_wifiOk) {
            s_wifiOk = wifiConnect();
        }
        if (s_wifiOk && !s_ntpOk) {
            s_ntpOk = ntpInit();
        }
    }

    delay(5);
}
