#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>
#include <math.h>
#include "secrets.h"

// Swiss station clock (Mondaine/SBB style) for ESP32 CYD (Cheap Yellow Display)
// Display: ST7789 320x240, LVGL v8, TFT_eSPI
// Time source: NTP via ntp.emi.u-bordeaux.fr, Europe/Paris timezone with DST
// Layout: analog clock face 240x240 (left) + info panel 80x240 (right)

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

// ── Clock geometry ────────────────────────────────────────────────────────────
#ifndef M_PI
#define M_PI   3.14159265358979323846f
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923f
#endif

#define CANVAS_W 200
#define CANVAS_H 200
#define CX 100   // canvas pivot x (centre du cadran)
#define CY 100   // canvas pivot y

// ── Canvas 240×240 pour le cadran + aiguilles ─────────────────────────────────
// Alloué en heap (malloc) pour éviter le débordement de la section BSS/DRAM
static lv_color_t *canvas_buf    = nullptr;    // 80 000 octets alloués dans setup() (200×200×2)
static lv_obj_t   *clock_canvas  = nullptr;

// ── Labels du panneau droit ───────────────────────────────────────────────────
static lv_obj_t *lbl_time   = nullptr;
static lv_obj_t *lbl_date   = nullptr;
static lv_obj_t *lbl_wifi   = nullptr;
static lv_obj_t *lbl_ntp    = nullptr;
static lv_obj_t *lbl_ip     = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// Display flush — lien LVGL → TFT_eSPI (conservé à l'identique)
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
// Init matériel
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
    // Tout éteint au départ (common-anode = HIGH pour off)
    ledcWrite(RED_CH,   MAX_PWM);
    ledcWrite(GREEN_CH, MAX_PWM);
    ledcWrite(BLUE_CH,  MAX_PWM);
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
    Serial.println("[WiFi] Echec de connexion (timeout 30s)");
    return false;
}

static void initNTP() {
    // Europe/Paris avec gestion DST automatique (heure d'été/hiver)
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "fr.pool.ntp.org");
}

static bool isNtpSynced() {
    // Le statut SNTP_SYNC_STATUS_COMPLETED est éphémère : il se réinitialise
    // après chaque cycle de synchronisation. On le latte dès la première fois.
    static bool _synced = false;
    if (!_synced) {
        _synced = sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
    }
    return _synced;
}

// ─────────────────────────────────────────────────────────────────────────────
// LED statut (non-bloquant, appelé toutes les 500 ms)
// ─────────────────────────────────────────────────────────────────────────────
static void setLedColor(bool red, bool green, bool blue) {
    ledcWrite(RED_CH,   red   ? 0 : MAX_PWM);
    ledcWrite(GREEN_CH, green ? 0 : MAX_PWM);
    ledcWrite(BLUE_CH,  blue  ? 0 : MAX_PWM);
}

static void updateStatusLed() {
    if (WiFi.status() != WL_CONNECTED) {
        setLedColor(true, false, false);    // rouge : pas de WiFi
    } else if (!isNtpSynced()) {
        setLedColor(false, true, false);    // vert  : WiFi ok, NTP en attente
    } else {
        setLedColor(false, false, true);    // bleu  : NTP synchronisé
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Dessin du cadran et des aiguilles sur le canvas
// ─────────────────────────────────────────────────────────────────────────────

// Dessine une aiguille depuis le pivot (CX,CY) avec angle, longueur et épaisseur donnés
static void drawHand(float angle, int length, int width, lv_color_t color, bool roundCaps) {
    int x2 = CX + (int)(cosf(angle) * length);
    int y2 = CY + (int)(sinf(angle) * length);

    lv_point_t pts[2] = {
        {(lv_coord_t)CX, (lv_coord_t)CY},
        {(lv_coord_t)x2, (lv_coord_t)y2}
    };
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color       = color;
    dsc.width       = width;
    dsc.opa         = LV_OPA_COVER;
    dsc.round_start = roundCaps ? 1 : 0;
    dsc.round_end   = roundCaps ? 1 : 0;
    lv_canvas_draw_line(clock_canvas, pts, 2, &dsc);
}

// Efface le canvas et redessine l'intégralité du cadran + aiguilles
static void drawClockFace(int h, int m, int s) {
    // 1. Fond noir
    lv_canvas_fill_bg(clock_canvas, lv_color_make(0, 0, 0), LV_OPA_COVER);

    // 2. Anneau extérieur (cercle)
    {
        lv_draw_arc_dsc_t dsc;
        lv_draw_arc_dsc_init(&dsc);
        dsc.color = lv_color_make(255, 255, 255);
        dsc.width = 3;
        dsc.opa   = LV_OPA_COVER;
        lv_canvas_draw_arc(clock_canvas, CX, CY, 93, 0, 360, &dsc);
    }

    // 3. Index horaires (12 épais) et minutes (48 fins)
    for (int i = 0; i < 60; i++) {
        float angle  = (i / 60.0f) * 2.0f * M_PI - M_PI_2;
        bool  isHour = (i % 5 == 0);
        int   innerR = isHour ? 80 : 86;
        int   outerR = 90;
        int   thick  = isHour ? 4 : 2;

        lv_point_t pts[2] = {
            { (lv_coord_t)(CX + (int)(cosf(angle) * innerR)),
              (lv_coord_t)(CY + (int)(sinf(angle) * innerR)) },
            { (lv_coord_t)(CX + (int)(cosf(angle) * outerR)),
              (lv_coord_t)(CY + (int)(sinf(angle) * outerR)) }
        };
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = lv_color_make(255, 255, 255);
        dsc.width = thick;
        dsc.opa   = LV_OPA_COVER;
        lv_canvas_draw_line(clock_canvas, pts, 2, &dsc);
    }

    // 4. Calcul des angles (12h = -π/2)
    float h_ang = ((h % 12) / 12.0f + m / 720.0f) * 2.0f * M_PI - M_PI_2;
    float m_ang = (m / 60.0f  + s / 3600.0f)       * 2.0f * M_PI - M_PI_2;
    float s_ang = (s / 60.0f)                        * 2.0f * M_PI - M_PI_2;

    // 5. Aiguille des heures : blanche, épaisse (6px), courte (50px)
    drawHand(h_ang, 50, 6, lv_color_make(255, 255, 255), true);

    // 6. Aiguille des minutes : blanche, moyenne (4px), longue (75px)
    drawHand(m_ang, 75, 4, lv_color_make(255, 255, 255), true);

    // 7. Trotteuse : rouge, fine (2px), longue (83px)
    drawHand(s_ang, 83, 2, lv_color_make(220, 0, 0), false);

    // 8. Disque rouge à l'extrémité de la trotteuse (signature Mondaine)
    {
        int sx = CX + (int)(cosf(s_ang) * 83);
        int sy = CY + (int)(sinf(s_ang) * 83);
        lv_draw_rect_dsc_t rdsc;
        lv_draw_rect_dsc_init(&rdsc);
        rdsc.bg_color     = lv_color_make(220, 0, 0);
        rdsc.bg_opa       = LV_OPA_COVER;
        rdsc.radius       = LV_RADIUS_CIRCLE;
        rdsc.border_width = 0;
        lv_canvas_draw_rect(clock_canvas, sx - 4, sy - 4, 8, 8, &rdsc);
    }

    // 9. Pivot central (petit disque noir)
    {
        lv_draw_rect_dsc_t pdsc;
        lv_draw_rect_dsc_init(&pdsc);
        pdsc.bg_color     = lv_color_make(200, 200, 200);
        pdsc.bg_opa       = LV_OPA_COVER;
        pdsc.radius       = LV_RADIUS_CIRCLE;
        pdsc.border_width = 0;
        lv_canvas_draw_rect(clock_canvas, CX - 4, CY - 4, 8, 8, &pdsc);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction de l'UI LVGL
// ─────────────────────────────────────────────────────────────────────────────
static void buildUI() {
    lv_obj_t *screen = lv_scr_act();

    // Fond d'écran noir (la zone droite de 80px ne sera pas couverte par le canvas)
    lv_obj_set_style_bg_color(screen, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen,   LV_OPA_COVER,           LV_PART_MAIN);

    // Allocation heap du buffer canvas (hors BSS — évite le débordement DRAM)
    if (canvas_buf == nullptr) {
        canvas_buf = (lv_color_t *)malloc(CANVAS_W * CANVAS_H * sizeof(lv_color_t));
        if (canvas_buf == nullptr) {
            Serial.println("ERREUR: impossible d'allouer le buffer canvas!");
            return;
        }
        memset(canvas_buf, 0x00, CANVAS_W * CANVAS_H * sizeof(lv_color_t)); // noir
    }

    // Canvas 240×240 (cadran + aiguilles) — positionné à gauche
    clock_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(clock_canvas, canvas_buf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(clock_canvas, 20, 20);  // centré dans la zone gauche 240px
    lv_obj_set_style_border_width(clock_canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(clock_canvas, 0, LV_PART_MAIN);

    // Premier rendu (cadran vide)
    drawClockFace(0, 0, 0);

    // Panneau droit 80×240 — conteneur pour les labels
    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 80, 240);
    lv_obj_set_pos(panel, 240, 0);
    lv_obj_set_style_bg_color(panel,     lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel,       LV_OPA_COVER,           LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0,                       LV_PART_MAIN);
    lv_obj_set_style_radius(panel,       0,                       LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel,      0,                       LV_PART_MAIN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // --- WiFi ---
    lv_obj_t *lbl_wifi_title = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_wifi_title, lv_color_make(110, 110, 110), LV_PART_MAIN);
    lv_label_set_text(lbl_wifi_title, "WiFi:");
    lv_obj_set_pos(lbl_wifi_title, 4, 5);

    lbl_wifi = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_wifi, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(lbl_wifi, "...");
    lv_obj_set_pos(lbl_wifi, 4, 22);

    lbl_ip = lv_label_create(panel);
    lv_label_set_long_mode(lbl_ip, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl_ip, 78);
    lv_obj_set_style_text_color(lbl_ip, lv_color_make(80, 160, 80), LV_PART_MAIN);
    lv_label_set_text(lbl_ip, "---");
    lv_obj_set_pos(lbl_ip, 4, 40);

    // --- NTP ---
    lv_obj_t *lbl_ntp_title = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_ntp_title, lv_color_make(110, 110, 110), LV_PART_MAIN);
    lv_label_set_text(lbl_ntp_title, "NTP:");
    lv_obj_set_pos(lbl_ntp_title, 4, 65);

    lbl_ntp = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_ntp, lv_color_make(200, 150, 0), LV_PART_MAIN);
    lv_label_set_text(lbl_ntp, "attente");
    lv_obj_set_pos(lbl_ntp, 4, 82);

    // --- Heure et date ---
    lbl_time = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(lbl_time, "--:--:--");
    lv_obj_set_pos(lbl_time, 2, 120);

    lbl_date = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_date, lv_color_make(170, 170, 170), LV_PART_MAIN);
    lv_label_set_text(lbl_date, "--/--/--");
    lv_obj_set_pos(lbl_date, 2, 145);
}

// ─────────────────────────────────────────────────────────────────────────────
// Mise à jour du panneau droit
// ─────────────────────────────────────────────────────────────────────────────
static void updateInfoPanel(struct tm *t) {
    // Heure et date
    if (!isNtpSynced() || t == nullptr) {
        lv_label_set_text(lbl_time, "--:--:--");
        lv_label_set_text(lbl_date, "--/--/--");
    } else {
        char buf_t[16], buf_d[16];
        snprintf(buf_t, sizeof(buf_t), "%02d:%02d:%02d",
                 t->tm_hour, t->tm_min, t->tm_sec);
        snprintf(buf_d, sizeof(buf_d), "%02d/%02d/%02d",
                 t->tm_mday, t->tm_mon + 1, t->tm_year % 100);
        lv_label_set_text(lbl_time, buf_t);
        lv_label_set_text(lbl_date, buf_d);
    }

    // WiFi
    if (WiFi.status() == WL_CONNECTED) {
        lv_obj_set_style_text_color(lbl_wifi, lv_color_make(0, 210, 0), LV_PART_MAIN);
        lv_label_set_text(lbl_wifi, "OK");
        lv_label_set_text(lbl_ip, WiFi.localIP().toString().c_str());
    } else {
        lv_obj_set_style_text_color(lbl_wifi, lv_color_make(220, 40, 40), LV_PART_MAIN);
        lv_label_set_text(lbl_wifi, "FAIL");
        lv_label_set_text(lbl_ip, "---");
    }

    // NTP
    if (isNtpSynced()) {
        lv_obj_set_style_text_color(lbl_ntp, lv_color_make(0, 180, 255), LV_PART_MAIN);
        lv_label_set_text(lbl_ntp, "synchro");
    } else {
        lv_obj_set_style_text_color(lbl_ntp, lv_color_make(200, 150, 0), LV_PART_MAIN);
        lv_label_set_text(lbl_ntp, "attente");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    turnOnBacklight();
    initLeds();
    setLedColor(true, false, false);    // rouge : démarrage

    // TFT (séquence identique à l'original)
    tft.begin();
    tft.setRotation(1);
    tft.invertDisplay(INVERT_DISPLAY == 1);
    tft.fillScreen(TFT_BLACK);

    // LVGL
    initLVGL();
    buildUI();
    lv_timer_handler();     // premier rendu

    // WiFi + NTP (bloquant max 30 s)
    bool wifiOk = initWifi();
    if (wifiOk) {
        setLedColor(false, true, false);    // vert : WiFi connecté
        lv_obj_set_style_text_color(lbl_wifi, lv_color_make(0, 210, 0), LV_PART_MAIN);
        lv_label_set_text(lbl_wifi, "OK");
        lv_label_set_text(lbl_ip, WiFi.localIP().toString().c_str());
        lv_timer_handler();
        Serial.println("[NTP] Configuration: ntp.emi.u-bordeaux.fr (Europe/Paris DST)");
        initNTP();
        Serial.println("[NTP] En attente de synchronisation...");
    } else {
        lv_obj_set_style_text_color(lbl_wifi, lv_color_make(220, 40, 40), LV_PART_MAIN);
        lv_label_set_text(lbl_wifi, "FAIL");
        lv_timer_handler();
        Serial.println("[setup] Pas de WiFi - horloge non synchronisee");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// loop
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    static uint32_t prev_lvgl    = millis();
    static uint32_t prev_led     = millis();
    static uint32_t prev_status  = 0;
    static int      prev_sec     = -1;
    static bool     wasNtpSynced = false;

    // ── LVGL tick (conservé identique à l'original) ──
    uint32_t now     = millis();
    uint32_t elapsed = now - prev_lvgl;
    if (elapsed >= LVGL_TICK_PERIOD_MS) {
        lv_tick_inc(elapsed);
        prev_lvgl = now;
    }

    // ── Heure locale ──
    time_t    t_raw    = time(nullptr);
    struct tm *t       = localtime(&t_raw);
    bool       synced  = isNtpSynced();

    // ── Première synchronisation NTP ──
    if (synced && !wasNtpSynced) {
        wasNtpSynced = true;
        setLedColor(false, false, true);    // bleu : NTP OK
        Serial.printf("[NTP] Synchronise! Heure: %02d:%02d:%02d le %02d/%02d/%04d\n",
                      t->tm_hour, t->tm_min, t->tm_sec,
                      t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    }

    // ── Log de statut toutes les 60 s ──
    if (now - prev_status >= 60000) {
        prev_status = now;
        Serial.printf("[status] WiFi:%s NTP:%s Heure:%02d:%02d:%02d\n",
                      WiFi.status() == WL_CONNECTED ? "OK" : "NOK",
                      synced ? "OK" : "NOK",
                      t->tm_hour, t->tm_min, t->tm_sec);
    }

    // ── Mise à jour affichage une fois par seconde ──
    if (t->tm_sec != prev_sec) {
        prev_sec = t->tm_sec;
        drawClockFace(synced ? t->tm_hour : 0,
                      synced ? t->tm_min  : 0,
                      synced ? t->tm_sec  : 0);
        updateInfoPanel(t);
    }

    // ── LED toutes les 500 ms (non-bloquant) ──
    if (millis() - prev_led >= 500) {
        prev_led = millis();
        updateStatusLed();
    }

    lv_timer_handler();
}

