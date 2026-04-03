#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>
#include "secrets.h"
#include "app_mode.h"
#include "mpu.h"
#include "touch_mgr.h"
#include "ui_clock.h"
#include "ui_countdown.h"
#include "ui_stub.h"

// 4way-display — ESP32 CYD, LVGL v8, MPU6050 orientation-driven multi-mode display
// Modes: Clock | Countdown | Mode C | Mode D

// ── Shared globals (declared extern in app_mode.h) ────────────────────────────
uint8_t  g_active_rotation = 1;
uint32_t g_countdown_ms    = 0;
CdState  g_cd_state        = CD_IDLE;
AppMode  g_current_mode    = MODE_CLOCK;

// ── Display ───────────────────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();

static lv_disp_draw_buf_t draw_buf;
static lv_color_t         disp_buf[LV_HOR_RES_MAX * 10];
static lv_disp_drv_t      disp_drv;

#define LVGL_TICK_PERIOD_MS 1

// ── LED (common-anode: active LOW) ────────────────────────────────────────────
#define RED_LED_PIN   4
#define GREEN_LED_PIN 16
#define BLUE_LED_PIN  17
#define RED_CH        0
#define GREEN_CH      1
#define BLUE_CH       2
#define MAX_PWM       255

// ─────────────────────────────────────────────────────────────────────────────
// Display flush
// ─────────────────────────────────────────────────────────────────────────────
static void cyd_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)color_p, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

// ─────────────────────────────────────────────────────────────────────────────
// Hardware init
// ─────────────────────────────────────────────────────────────────────────────
static void turnOnBacklight() {
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
}

static void initLVGL() {
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, disp_buf, NULL, LV_HOR_RES_MAX * 10);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = cyd_disp_flush;
    disp_drv.sw_rotate = 1;
    disp_drv.rotated = LV_DISP_ROT_NONE;
    disp_drv.hor_res  = 320;
    disp_drv.ver_res  = 240;
    lv_disp_drv_register(&disp_drv);
}

static void initLeds() {
    ledcSetup(RED_CH,   5000, 8);
    ledcSetup(GREEN_CH, 5000, 8);
    ledcSetup(BLUE_CH,  5000, 8);
    ledcAttachPin(RED_LED_PIN,   RED_CH);
    ledcAttachPin(GREEN_LED_PIN, GREEN_CH);
    ledcAttachPin(BLUE_LED_PIN,  BLUE_CH);
    ledcWrite(RED_CH,   MAX_PWM);
    ledcWrite(GREEN_CH, MAX_PWM);
    ledcWrite(BLUE_CH,  MAX_PWM);
}

// ─────────────────────────────────────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────────────────────────────────────
static void apply_mode(AppMode mode);
bool isNtpSynced();

// ─────────────────────────────────────────────────────────────────────────────
// LED status
// ─────────────────────────────────────────────────────────────────────────────
static void setLedColor(bool red, bool green, bool blue) {
    ledcWrite(RED_CH,   red   ? 0 : MAX_PWM);
    ledcWrite(GREEN_CH, green ? 0 : MAX_PWM);
    ledcWrite(BLUE_CH,  blue  ? 0 : MAX_PWM);
}

static void updateStatusLed(uint32_t now_ms) {
    static uint32_t prev_blink = 0;
    static bool     blink_on   = false;

    // CD_DONE overrides: red blinking
    if (g_cd_state == CD_DONE) {
        if (now_ms - prev_blink >= 500) {
            prev_blink = now_ms;
            blink_on   = !blink_on;
            setLedColor(blink_on, false, false);
        }
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        setLedColor(true, false, false);
    } else if (!isNtpSynced()) {
        setLedColor(false, true, false);
    } else {
        setLedColor(false, false, true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi + NTP
// ─────────────────────────────────────────────────────────────────────────────
static bool initWifi() {
    Serial.printf("[WiFi] Connexion a '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t start = millis();
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
        delay(200);
        if (++dots % 10 == 0) {
            Serial.printf("[WiFi] En attente... %ds (status=%d)\n",
                          (int)((millis() - start) / 1000), WiFi.status());
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connecte! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WiFi] Echec connexion (timeout 30s)");
    return false;
}

bool isNtpSynced() {
    static bool _synced = false;
    if (!_synced) _synced = sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
    return _synced;
}

static void initNTP() {
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "fr.pool.ntp.org");
}

// ─────────────────────────────────────────────────────────────────────────────
// Mode switch
// ─────────────────────────────────────────────────────────────────────────────
// Hardware TFT rotation per mode.
// MODE_COUNTDOWN intentionally stays on rotation=1 because this ST7789 panel
// corrupts partial updates in hardware rotation=3 with the current offset setup.
static const uint8_t MODE_ROTATION[4] = {
    1,  // MODE_CLOCK      → landscape 320×240
    1,  // MODE_COUNTDOWN  → landscape 320×240 (LVGL applies 180° software rotation)
    0,  // MODE_C          → portrait 240×320
    2,  // MODE_D          → portrait inv 240×320
};

static const lv_disp_rot_t MODE_LVGL_ROTATION[4] = {
    LV_DISP_ROT_NONE,
    LV_DISP_ROT_180,
    LV_DISP_ROT_NONE,
    LV_DISP_ROT_NONE,
};

static void apply_mode(AppMode mode) {
    uint8_t rot = MODE_ROTATION[mode];
    g_active_rotation = rot;
    g_current_mode    = mode;

    tft.setRotation(rot);
    tft.fillScreen(TFT_BLACK);

    bool landscape   = (rot % 2 == 1);
    disp_drv.hor_res = landscape ? 320 : 240;
    disp_drv.ver_res = landscape ? 240 : 320;
    disp_drv.rotated = MODE_LVGL_ROTATION[mode];
    lv_disp_drv_update(lv_disp_get_default(), &disp_drv);

    ui_clock_reset_refs();
    ui_countdown_reset_refs();
    lv_obj_clean(lv_scr_act());

    switch (mode) {
        case MODE_CLOCK:      ui_clock_build();               break;
        case MODE_COUNTDOWN:  ui_countdown_build();           break;
        case MODE_C:          ui_stub_build("Mode C");        break;
        case MODE_D:          ui_stub_build("Mode D");        break;
    }
    lv_timer_handler();
    Serial.printf("[Mode] Switch -> mode=%d rotation=%d\n", (int)mode, rot);
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    turnOnBacklight();
    initLeds();
    setLedColor(true, false, false);

    tft.begin();
    tft.setRotation(1);
    tft.invertDisplay(INVERT_DISPLAY == 1);
    tft.fillScreen(TFT_BLACK);

    mpu_init();

    initLVGL();
    touch_init();
    apply_mode(MODE_CLOCK);

    bool wifiOk = initWifi();
    if (wifiOk) {
        setLedColor(false, true, false);
        ui_clock_refresh_status(true, WiFi.localIP().toString().c_str());
        lv_timer_handler();
        Serial.println("[NTP] Configuration: fr.pool.ntp.org (Europe/Paris DST)");
        initNTP();
        Serial.println("[NTP] En attente de synchronisation...");
    } else {
        ui_clock_refresh_status(false, "---");
        lv_timer_handler();
        Serial.println("[Setup] Pas de WiFi - horloge non synchronisee");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// loop
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    static uint32_t prev_lvgl   = millis();
    static uint32_t prev_led    = millis();
    static uint32_t prev_mpu    = millis();
    static uint32_t prev_status = 0;
    static int      prev_sec    = -1;
    static bool     was_synced  = false;

    uint32_t now = millis();

    // ── LVGL tick ──
    uint32_t elapsed = now - prev_lvgl;
    if (elapsed >= LVGL_TICK_PERIOD_MS) {
        lv_tick_inc(elapsed);
        prev_lvgl = now;
    }

    // ── Countdown background tick ──
    countdown_tick(now);

    // ── Orientation / mode switch every ~50ms ──
    if (now - prev_mpu >= 50) {
        prev_mpu = now;
        if (mpu_update(now)) {
            apply_mode(mpu_current_mode());
        }
    }

    // ── Time ──
    time_t    t_raw  = time(nullptr);
    struct tm *t     = localtime(&t_raw);
    bool       synced = isNtpSynced();

    // ── First NTP sync ──
    if (synced && !was_synced) {
        was_synced = true;
        setLedColor(false, false, true);
        Serial.printf("[NTP] Synchronise! %02d:%02d:%02d le %02d/%02d/%04d\n",
                      t->tm_hour, t->tm_min, t->tm_sec,
                      t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    }

    // ── Periodic status log ──
    if (now - prev_status >= 60000) {
        prev_status = now;
        Serial.printf("[status] WiFi:%s NTP:%s Mode:%d Heure:%02d:%02d:%02d\n",
                      WiFi.status() == WL_CONNECTED ? "OK" : "NOK",
                      synced ? "OK" : "NOK",
                      (int)g_current_mode,
                      t->tm_hour, t->tm_min, t->tm_sec);
    }

    // ── Per-second UI update ──
    if (t->tm_sec != prev_sec) {
        prev_sec = t->tm_sec;
        if (g_current_mode == MODE_CLOCK) {
            ui_clock_update(t, synced);
        } else if (g_current_mode == MODE_COUNTDOWN) {
            ui_countdown_update();
        }
    }

    // ── LED every 500ms ──
    if (now - prev_led >= 500) {
        prev_led = now;
        updateStatusLed(now);
    }

    lv_timer_handler();
}

