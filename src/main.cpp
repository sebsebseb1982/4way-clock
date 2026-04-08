#include <Arduino.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>
#include "secrets.h"
#include "app_mode.h"
#include "mpu.h"
#include "touch_mgr.h"
#include "ui_clock.h"
#include "ui_countdown.h"
#include "ui_wheel.h"
#include "ui_stub.h"

// 4way-display — ESP32 CYD, LVGL v8, MPU6500 orientation-driven multi-mode display
// Modes: Clock | Countdown | Wheel | Mode D

// ── Shared globals (declared extern in app_mode.h) ────────────────────────────
uint8_t  g_active_rotation = 1;
uint32_t g_countdown_ms    = 0;
CdState  g_cd_state        = CD_IDLE;
AppMode  g_current_mode    = MODE_CLOCK;

// ── Display — ESP32-4848S040 : ST7701S via RGB parallel, backlight GPIO2 ────
#define GFX_BL 38

static Arduino_DataBus       *s_panel_init_bus = nullptr;
static Arduino_ESP32RGBPanel *s_bus = nullptr;
static Arduino_RGB_Display   *gfx   = nullptr;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t        *disp_buf = nullptr;   // allocated in PSRAM
static lv_disp_drv_t      disp_drv;
static uint32_t           s_flush_count = 0;

#define LVGL_TICK_PERIOD_MS 1

// ── LED — GPIO4/16/17 are display data pins on ESP32-4848S040 ────────────────
// No dedicated RGB LED; status is reported via Serial only.
#define MAX_PWM 255  // kept for updateStatusLed signature compatibility

// ── Buzzer — GPIO40, intermittent alarm for countdown completion ─────────────
#define BUZZER_PIN 40
#define BUZZER_PWM_CHANNEL 0
#define BUZZER_FREQUENCY_HZ 1000
#define BUZZER_NAV_TICK_FREQUENCY_HZ 1800
#define BUZZER_ALERT_DURATION_MS 20000
#define BUZZER_BEEP_ON_MS 400
#define BUZZER_BEEP_OFF_MS 200
#define BUZZER_NAV_TICK_MS 18
#define BUZZER_NAV_TICK_GAP_MS 35

static bool     s_buzzer_alert_active = false;
static bool     s_buzzer_output_on    = false;
static uint32_t s_buzzer_alert_start  = 0;
static uint32_t s_buzzer_phase_start  = 0;
static bool     s_buzzer_nav_tick_active = false;
static uint32_t s_buzzer_nav_tick_start  = 0;
static uint32_t s_buzzer_nav_tick_last   = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Display flush
// ─────────────────────────────────────────────────────────────────────────────
static void s3_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    if (s_flush_count < 5) {
        Serial.printf("[LVGL] flush #%lu area=(%d,%d)-(%d,%d) size=%lux%lu buf=%p\n",
                      (unsigned long)(s_flush_count + 1),
                      area->x1, area->y1, area->x2, area->y2,
                      (unsigned long)w, (unsigned long)h, color_p);
    }
    s_flush_count++;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
    lv_disp_flush_ready(drv);
}

// ─────────────────────────────────────────────────────────────────────────────
// Hardware init
// ─────────────────────────────────────────────────────────────────────────────
static void initDisplay() {
    Serial.println("[Display] initDisplay() start");

    // Backlight on before RGB data flows to avoid a white flash
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, LOW);

    // ── ST7701S serial init bus + RGB parallel bus — official 86BOX pinout ─
    s_panel_init_bus = new Arduino_SWSPI(
        GFX_NOT_DEFINED /* DC */, 39 /* CS */,
        48 /* SCK */, 47 /* MOSI */, GFX_NOT_DEFINED /* MISO */
    );

    s_bus = new Arduino_ESP32RGBPanel(
        18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
        11, 12, 13, 14, 0, /* R0-R4 */
        8, 20, 3, 46, 9, 10, /* G0-G5 */
        4, 5, 6, 7, 15, /* B0-B4 */
        1 /* hsync_polarity */, 10 /* hsync_fp */, 8 /* hsync_pw */, 50 /* hsync_bp */,
        1 /* vsync_polarity */, 10 /* vsync_fp */, 8 /* vsync_pw */, 20 /* vsync_bp */
    );
    gfx = new Arduino_RGB_Display(
        480, 480, s_bus, 1 /* rotation */, true /* auto_flush */,
        s_panel_init_bus, GFX_NOT_DEFINED /* RST */,
        st7701_type9_init_operations, sizeof(st7701_type9_init_operations)
    );
    Serial.printf("[Display] init_bus=%p rgb_bus=%p gfx=%p bl=%d\n",
                  s_panel_init_bus, s_bus, gfx, GFX_BL);
    if (!gfx->begin()) {
        Serial.println("[Display] ERREUR: gfx->begin() a echoue");
    } else {
        Serial.println("[Display] gfx->begin() OK");
    }

    Serial.println("[Display] test pattern direct RGB start");
    gfx->fillScreen(BLACK);
    gfx->fillRect(0,   0,   160, 160, RED);
    gfx->fillRect(160, 0,   160, 160, GREEN);
    gfx->fillRect(320, 0,   160, 160, BLUE);
    gfx->fillRect(0,   160, 480, 160, WHITE);
    gfx->fillRect(0,   320, 480, 160, BLACK);
    delay(250);

    gfx->fillScreen(BLACK);
    digitalWrite(GFX_BL, HIGH);   // backlight on once frame is ready
    Serial.println("[Display] backlight ON, screen cleared to black");
}

static void initLVGL() {
    // Allocate LVGL draw buffer in PSRAM (1/10 frame = 480×48 pixels)
    size_t buf_pixels = LVGL_DISPLAY_WIDTH * (LVGL_DISPLAY_HEIGHT / 10);
    disp_buf = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!disp_buf) {
        Serial.println("[LVGL] ERREUR: allocation PSRAM echouee, repli sur SRAM");
        static lv_color_t fallback[480 * 10];
        disp_buf = fallback;
        buf_pixels = 480 * 10;
    }

    Serial.printf("[LVGL] init start width=%d height=%d buf_pixels=%lu buf=%p\n",
                  LVGL_DISPLAY_WIDTH, LVGL_DISPLAY_HEIGHT,
                  (unsigned long)buf_pixels, disp_buf);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, disp_buf, NULL, buf_pixels);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = s3_disp_flush;
    disp_drv.sw_rotate = 1;
    disp_drv.rotated   = LV_DISP_ROT_NONE;
    disp_drv.hor_res   = LVGL_DISPLAY_WIDTH;
    disp_drv.ver_res   = LVGL_DISPLAY_HEIGHT;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    Serial.printf("[LVGL] display registered=%p sw_rotate=%d rotated=%d res=%dx%d\n",
                  disp, disp_drv.sw_rotate, disp_drv.rotated,
                  disp_drv.hor_res, disp_drv.ver_res);
    Serial.println("[LVGL] init complete");
}

static void initLeds() {
    // No RGB LED on ESP32-4848S040 — GPIO4/16/17 are display data lines.
    // LED status is reported via Serial output only.
}

static void setBuzzerOutput(bool enabled) {
    if (enabled == s_buzzer_output_on) return;

    if (enabled) {
        ledcWriteTone(BUZZER_PWM_CHANNEL, BUZZER_FREQUENCY_HZ);
    } else {
        ledcWriteTone(BUZZER_PWM_CHANNEL, 0);
        digitalWrite(BUZZER_PIN, LOW);
    }

    s_buzzer_output_on = enabled;
}

static void startBuzzerTone(uint32_t frequency_hz) {
    ledcWriteTone(BUZZER_PWM_CHANNEL, frequency_hz);
    s_buzzer_output_on = frequency_hz != 0;
}

static void stopBuzzerAlert() {
    s_buzzer_alert_active = false;
    s_buzzer_alert_start = 0;
    s_buzzer_phase_start = 0;
    setBuzzerOutput(false);
}

void buzzer_request_navigation_tick(uint32_t now_ms) {
    if (s_buzzer_alert_active || g_cd_state == CD_DONE) return;
    if (now_ms - s_buzzer_nav_tick_last < BUZZER_NAV_TICK_GAP_MS) return;

    s_buzzer_nav_tick_last = now_ms;
    s_buzzer_nav_tick_start = now_ms;
    s_buzzer_nav_tick_active = true;
    startBuzzerTone(BUZZER_NAV_TICK_FREQUENCY_HZ);
}

static void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    ledcSetup(BUZZER_PWM_CHANNEL, BUZZER_FREQUENCY_HZ, 8);
    ledcAttachPin(BUZZER_PIN, BUZZER_PWM_CHANNEL);
    setBuzzerOutput(false);
    Serial.printf("[Buzzer] init pin=%d channel=%d freq=%dHz\n",
                  BUZZER_PIN, BUZZER_PWM_CHANNEL, BUZZER_FREQUENCY_HZ);
}

static void updateBuzzer(uint32_t now_ms) {
    static CdState prev_cd_state = CD_IDLE;

    if (g_cd_state == CD_DONE && prev_cd_state != CD_DONE) {
        s_buzzer_nav_tick_active = false;
        s_buzzer_alert_active = true;
        s_buzzer_alert_start = now_ms;
        s_buzzer_phase_start = now_ms;
        setBuzzerOutput(true);
        Serial.printf("[Buzzer] countdown complete, alarm started for %lu ms\n",
                      (unsigned long)BUZZER_ALERT_DURATION_MS);
    }
    prev_cd_state = g_cd_state;

    if (g_cd_state != CD_DONE && s_buzzer_alert_active) {
        stopBuzzerAlert();
    }

    if (s_buzzer_alert_active) {
        if (now_ms - s_buzzer_alert_start >= BUZZER_ALERT_DURATION_MS) {
            stopBuzzerAlert();
            Serial.println("[Buzzer] alarm finished");
            return;
        }

        uint32_t phase_duration = s_buzzer_output_on ? BUZZER_BEEP_ON_MS : BUZZER_BEEP_OFF_MS;
        if (now_ms - s_buzzer_phase_start >= phase_duration) {
            s_buzzer_phase_start = now_ms;
            setBuzzerOutput(!s_buzzer_output_on);
        }
        return;
    }

    if (s_buzzer_nav_tick_active && now_ms - s_buzzer_nav_tick_start >= BUZZER_NAV_TICK_MS) {
        s_buzzer_nav_tick_active = false;
        setBuzzerOutput(false);
    }
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
    // No physical LED on ESP32-4848S040; no-op.
    (void)red; (void)green; (void)blue;
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
// Display is 480×480 (square): hor_res/ver_res are always 480 regardless of rotation.
// Hardware TFT rotation is not needed; LVGL sw_rotate handles all orientations.
static const lv_disp_rot_t MODE_LVGL_ROTATION[4] = {
    LV_DISP_ROT_NONE,   // MODE_CLOCK
    LV_DISP_ROT_180,    // MODE_COUNTDOWN (UI drawn for opposite face)
    LV_DISP_ROT_270,    // MODE_WHEEL
    LV_DISP_ROT_90,     // MODE_D
};

static void apply_mode(AppMode mode) {
    Serial.printf("[Mode] apply_mode(%d) start\n", (int)mode);
    g_active_rotation = (uint8_t)mode;   // used as rotation hint by touch
    g_current_mode    = mode;

    touch_set_rotation((uint8_t)mode);

    // Square display — resolution stays 480×480 for every rotation
    disp_drv.rotated = MODE_LVGL_ROTATION[mode];
    lv_disp_drv_update(lv_disp_get_default(), &disp_drv);

    ui_clock_reset_refs();
    ui_countdown_reset_refs();
    ui_wheel_reset_refs();
    lv_obj_clean(lv_scr_act());

    switch (mode) {
        case MODE_CLOCK:      ui_clock_build();               break;
        case MODE_COUNTDOWN:  ui_countdown_build();           break;
        case MODE_WHEEL:      ui_wheel_build();               break;
        case MODE_D:          ui_stub_build("Mode D");        break;
    }
    Serial.printf("[Mode] screen=%p children=%u\n",
                  lv_scr_act(), (unsigned)lv_obj_get_child_cnt(lv_scr_act()));
    lv_obj_update_layout(lv_scr_act());
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(lv_disp_get_default());
    lv_timer_handler();
    Serial.printf("[Mode] Switch -> mode=%d rotation=%d\n", (int)mode, (int)MODE_LVGL_ROTATION[mode]);
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[Setup] boot start");

    initDisplay();   // RGB panel + backlight
    Serial.println("[Setup] display init complete");
    initLeds();
    initBuzzer();
    setLedColor(true, false, false);

    if (!mpu_init()) {
        Serial.println("[Setup] MPU indisponible - bascule d'orientation desactivee");
    } else {
        Serial.println("[Setup] MPU init complete");
    }

    initLVGL();
    Serial.println("[Setup] LVGL init complete");
    touch_init();
    Serial.println("[Setup] touch init complete");
    apply_mode(MODE_CLOCK);
    Serial.println("[Setup] MODE_CLOCK applied");

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
    static bool     logged_clock_tick = false;
    static uint32_t prev_loop_log = 0;

    uint32_t now = millis();

    // ── LVGL tick ──
    uint32_t elapsed = now - prev_lvgl;
    if (elapsed >= LVGL_TICK_PERIOD_MS) {
        lv_tick_inc(elapsed);
        prev_lvgl = now;
    }

    // ── Countdown background tick ──
    countdown_tick(now);
    updateBuzzer(now);

    if (g_current_mode == MODE_WHEEL) {
        ui_wheel_tick(now);
    }

    // ── Orientation / mode switch via MPU6500 ──
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

    if (now - prev_loop_log >= 5000) {
        prev_loop_log = now;
        Serial.printf("[loop] alive now=%lu mode=%d lvgl_rot=%d countdown_ms=%lu cd_state=%d heap=%lu psram=%lu\n",
                      (unsigned long)now,
                      (int)g_current_mode,
                      (int)disp_drv.rotated,
                      (unsigned long)g_countdown_ms,
                      (int)g_cd_state,
                      (unsigned long)ESP.getFreeHeap(),
                      (unsigned long)ESP.getFreePsram());
    }

    // ── Per-second UI update ──
    if (t->tm_sec != prev_sec) {
        prev_sec = t->tm_sec;
        if (g_current_mode == MODE_CLOCK) {
            if (!logged_clock_tick) {
                Serial.printf("[Clock] first loop update synced=%d time=%02d:%02d:%02d\n",
                              (int)synced, t->tm_hour, t->tm_min, t->tm_sec);
                logged_clock_tick = true;
            }
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

