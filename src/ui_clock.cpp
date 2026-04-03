#include "ui_clock.h"
#include <lvgl.h>
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>

// forward declare — defined in main.cpp
extern bool isNtpSynced();

// ── Canvas geometry ───────────────────────────────────────────────────────────
#define CANVAS_W 200
#define CANVAS_H 200
#define CX 100
#define CY 100

#ifndef M_PI
#define M_PI   3.14159265358979323846f
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923f
#endif

// ── Module-private state ──────────────────────────────────────────────────────
static lv_color_t *s_canvas_buf   = nullptr;   // heap-allocated once, never freed
static lv_obj_t   *s_clock_canvas = nullptr;

static lv_obj_t *s_lbl_time = nullptr;
static lv_obj_t *s_lbl_date = nullptr;
static lv_obj_t *s_lbl_wifi = nullptr;
static lv_obj_t *s_lbl_ntp  = nullptr;
static lv_obj_t *s_lbl_ip   = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// Hand drawing
// ─────────────────────────────────────────────────────────────────────────────
static void draw_hand(float angle, int length, int width, lv_color_t color, bool round_caps) {
    int x2 = CX + (int)(cosf(angle) * length);
    int y2 = CY + (int)(sinf(angle) * length);

    lv_point_t pts[2] = {
        { (lv_coord_t)CX, (lv_coord_t)CY },
        { (lv_coord_t)x2, (lv_coord_t)y2 }
    };
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color       = color;
    dsc.width       = width;
    dsc.opa         = LV_OPA_COVER;
    dsc.round_start = round_caps ? 1 : 0;
    dsc.round_end   = round_caps ? 1 : 0;
    lv_canvas_draw_line(s_clock_canvas, pts, 2, &dsc);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw full clock face (background + markers + hands)
// ─────────────────────────────────────────────────────────────────────────────
static void draw_clock_face(int h, int m, int s) {
    lv_canvas_fill_bg(s_clock_canvas, lv_color_make(0, 0, 0), LV_OPA_COVER);

    // Outer ring
    {
        lv_draw_arc_dsc_t dsc;
        lv_draw_arc_dsc_init(&dsc);
        dsc.color = lv_color_make(255, 255, 255);
        dsc.width = 3;
        dsc.opa   = LV_OPA_COVER;
        lv_canvas_draw_arc(s_clock_canvas, CX, CY, 93, 0, 360, &dsc);
    }

    // Hour and minute markers
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
        lv_canvas_draw_line(s_clock_canvas, pts, 2, &dsc);
    }

    float h_ang = ((h % 12) / 12.0f + m / 720.0f) * 2.0f * M_PI - M_PI_2;
    float m_ang = (m / 60.0f + s / 3600.0f)        * 2.0f * M_PI - M_PI_2;
    float s_ang = (s / 60.0f)                        * 2.0f * M_PI - M_PI_2;

    draw_hand(h_ang, 50, 6, lv_color_make(255, 255, 255), true);
    draw_hand(m_ang, 75, 4, lv_color_make(255, 255, 255), true);
    draw_hand(s_ang, 83, 2, lv_color_make(220, 0, 0),     false);

    // Seconds tip disc (Mondaine signature)
    {
        int sx = CX + (int)(cosf(s_ang) * 83);
        int sy = CY + (int)(sinf(s_ang) * 83);
        lv_draw_rect_dsc_t rdsc;
        lv_draw_rect_dsc_init(&rdsc);
        rdsc.bg_color     = lv_color_make(220, 0, 0);
        rdsc.bg_opa       = LV_OPA_COVER;
        rdsc.radius       = LV_RADIUS_CIRCLE;
        rdsc.border_width = 0;
        lv_canvas_draw_rect(s_clock_canvas, sx - 4, sy - 4, 8, 8, &rdsc);
    }

    // Pivot
    {
        lv_draw_rect_dsc_t pdsc;
        lv_draw_rect_dsc_init(&pdsc);
        pdsc.bg_color     = lv_color_make(200, 200, 200);
        pdsc.bg_opa       = LV_OPA_COVER;
        pdsc.radius       = LV_RADIUS_CIRCLE;
        pdsc.border_width = 0;
        lv_canvas_draw_rect(s_clock_canvas, CX - 4, CY - 4, 8, 8, &pdsc);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Build clock UI
// ─────────────────────────────────────────────────────────────────────────────
void ui_clock_build() {
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    // Allocate canvas buffer once — never freed
    if (s_canvas_buf == nullptr) {
        s_canvas_buf = (lv_color_t *)malloc(CANVAS_W * CANVAS_H * sizeof(lv_color_t));
        if (s_canvas_buf == nullptr) {
            Serial.println("[Clock] ERREUR: malloc canvas_buf impossible!");
            return;
        }
        memset(s_canvas_buf, 0x00, CANVAS_W * CANVAS_H * sizeof(lv_color_t));
    }

    s_clock_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_clock_canvas, s_canvas_buf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_clock_canvas, 20, 20);
    lv_obj_set_style_border_width(s_clock_canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_clock_canvas, 0, LV_PART_MAIN);

    draw_clock_face(0, 0, 0);

    // Right info panel (80×240)
    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 80, 240);
    lv_obj_set_pos(panel, 240, 0);
    lv_obj_set_style_bg_color(panel,     lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel,       LV_OPA_COVER,           LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0,                       LV_PART_MAIN);
    lv_obj_set_style_radius(panel,       0,                       LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel,      0,                       LV_PART_MAIN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_wifi_title = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_wifi_title, lv_color_make(110, 110, 110), LV_PART_MAIN);
    lv_label_set_text(lbl_wifi_title, "WiFi:");
    lv_obj_set_pos(lbl_wifi_title, 4, 5);

    s_lbl_wifi = lv_label_create(panel);
    lv_obj_set_style_text_color(s_lbl_wifi, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(s_lbl_wifi, "...");
    lv_obj_set_pos(s_lbl_wifi, 4, 22);

    s_lbl_ip = lv_label_create(panel);
    lv_label_set_long_mode(s_lbl_ip, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(s_lbl_ip, 78);
    lv_obj_set_style_text_color(s_lbl_ip, lv_color_make(80, 160, 80), LV_PART_MAIN);
    lv_label_set_text(s_lbl_ip, "---");
    lv_obj_set_pos(s_lbl_ip, 4, 40);

    lv_obj_t *lbl_ntp_title = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_ntp_title, lv_color_make(110, 110, 110), LV_PART_MAIN);
    lv_label_set_text(lbl_ntp_title, "NTP:");
    lv_obj_set_pos(lbl_ntp_title, 4, 65);

    s_lbl_ntp = lv_label_create(panel);
    lv_obj_set_style_text_color(s_lbl_ntp, lv_color_make(200, 150, 0), LV_PART_MAIN);
    lv_label_set_text(s_lbl_ntp, "attente");
    lv_obj_set_pos(s_lbl_ntp, 4, 82);

    s_lbl_time = lv_label_create(panel);
    lv_obj_set_style_text_color(s_lbl_time, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(s_lbl_time, "--:--:--");
    lv_obj_set_pos(s_lbl_time, 2, 120);

    s_lbl_date = lv_label_create(panel);
    lv_obj_set_style_text_color(s_lbl_date, lv_color_make(170, 170, 170), LV_PART_MAIN);
    lv_label_set_text(s_lbl_date, "--/--/--");
    lv_obj_set_pos(s_lbl_date, 2, 145);
}

// ─────────────────────────────────────────────────────────────────────────────
// Update clock display (called every second when mode == MODE_CLOCK)
// ─────────────────────────────────────────────────────────────────────────────
void ui_clock_update(struct tm *t, bool synced) {
    draw_clock_face(synced ? t->tm_hour : 0,
                    synced ? t->tm_min  : 0,
                    synced ? t->tm_sec  : 0);

    if (!synced || t == nullptr) {
        lv_label_set_text(s_lbl_time, "--:--:--");
        lv_label_set_text(s_lbl_date, "--/--/--");
    } else {
        char buf_t[16], buf_d[16];
        snprintf(buf_t, sizeof(buf_t), "%02d:%02d:%02d",
                 t->tm_hour, t->tm_min, t->tm_sec);
        snprintf(buf_d, sizeof(buf_d), "%02d/%02d/%02d",
                 t->tm_mday, t->tm_mon + 1, t->tm_year % 100);
        lv_label_set_text(s_lbl_time, buf_t);
        lv_label_set_text(s_lbl_date, buf_d);
    }

    if (WiFi.status() == WL_CONNECTED) {
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_make(0, 210, 0), LV_PART_MAIN);
        lv_label_set_text(s_lbl_wifi, "OK");
        lv_label_set_text(s_lbl_ip, WiFi.localIP().toString().c_str());
    } else {
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_make(220, 40, 40), LV_PART_MAIN);
        lv_label_set_text(s_lbl_wifi, "FAIL");
        lv_label_set_text(s_lbl_ip, "---");
    }

    if (synced) {
        lv_obj_set_style_text_color(s_lbl_ntp, lv_color_make(0, 180, 255), LV_PART_MAIN);
        lv_label_set_text(s_lbl_ntp, "synchro");
    } else {
        lv_obj_set_style_text_color(s_lbl_ntp, lv_color_make(200, 150, 0), LV_PART_MAIN);
        lv_label_set_text(s_lbl_ntp, "attente");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ui_clock_refresh_status(bool wifi_ok, const char *ip) {
    if (s_lbl_wifi == nullptr) return;
    if (wifi_ok) {
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_make(0, 210, 0), LV_PART_MAIN);
        lv_label_set_text(s_lbl_wifi, "OK");
        lv_label_set_text(s_lbl_ip, ip);
    } else {
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_make(220, 40, 40), LV_PART_MAIN);
        lv_label_set_text(s_lbl_wifi, "FAIL");
        lv_label_set_text(s_lbl_ip, "---");
    }
}
