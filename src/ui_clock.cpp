#include "ui_clock.h"
#include <lvgl.h>
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>

// forward declare — defined in main.cpp
extern bool isNtpSynced();

// ── Canvas geometry ───────────────────────────────────────────────────────────
#define CANVAS_W 400
#define CANVAS_H 400
#define CX 200
#define CY 200

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
static uint32_t  s_clock_draw_count = 0;

void ui_clock_reset_refs() {
    s_clock_canvas = nullptr;
    s_lbl_time = nullptr;
    s_lbl_date = nullptr;
    s_lbl_wifi = nullptr;
    s_lbl_ntp = nullptr;
}

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
    if (s_clock_canvas == nullptr) {
        Serial.println("[Clock] ERREUR: draw_clock_face sans canvas");
        return;
    }

    lv_canvas_fill_bg(s_clock_canvas, lv_color_make(0, 0, 0), LV_OPA_COVER);

    // Outer ring
    {
        lv_draw_arc_dsc_t dsc;
        lv_draw_arc_dsc_init(&dsc);
        dsc.color = lv_color_make(255, 255, 255);
        dsc.width = 6;
        dsc.opa   = LV_OPA_COVER;
        lv_canvas_draw_arc(s_clock_canvas, CX, CY, 186, 0, 360, &dsc);
    }

    // Hour and minute markers
    for (int i = 0; i < 60; i++) {
        float angle  = (i / 60.0f) * 2.0f * M_PI - M_PI_2;
        bool  isHour = (i % 5 == 0);
        int   innerR = isHour ? 160 : 172;
        int   outerR = 180;
        int   thick  = isHour ? 8 : 4;

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

    draw_hand(h_ang, 100, 10, lv_color_make(255, 255, 255), true);
    draw_hand(m_ang, 145, 6, lv_color_make(255, 255, 255), true);
    draw_hand(s_ang, 162, 3, lv_color_make(220, 0, 0),     false);

    // Seconds tip disc (Mondaine signature)
    {
        int sx = CX + (int)(cosf(s_ang) * 162);
        int sy = CY + (int)(sinf(s_ang) * 162);
        lv_draw_rect_dsc_t rdsc;
        lv_draw_rect_dsc_init(&rdsc);
        rdsc.bg_color     = lv_color_make(220, 0, 0);
        rdsc.bg_opa       = LV_OPA_COVER;
        rdsc.radius       = LV_RADIUS_CIRCLE;
        rdsc.border_width = 0;
        lv_canvas_draw_rect(s_clock_canvas, sx - 6, sy - 6, 12, 12, &rdsc);
    }

    // Pivot
    {
        lv_draw_rect_dsc_t pdsc;
        lv_draw_rect_dsc_init(&pdsc);
        pdsc.bg_color     = lv_color_make(200, 200, 200);
        pdsc.bg_opa       = LV_OPA_COVER;
        pdsc.radius       = LV_RADIUS_CIRCLE;
        pdsc.border_width = 0;
        lv_canvas_draw_rect(s_clock_canvas, CX - 6, CY - 6, 12, 12, &pdsc);
    }

    lv_obj_invalidate(s_clock_canvas);
    if (s_clock_draw_count < 5) {
        Serial.printf("[Clock] draw #%lu h=%d m=%d s=%d canvas=%p\n",
                      (unsigned long)(s_clock_draw_count + 1), h, m, s, s_clock_canvas);
    }
    s_clock_draw_count++;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build clock UI
// ─────────────────────────────────────────────────────────────────────────────
void ui_clock_build() {
    lv_obj_t *screen = lv_scr_act();
    Serial.printf("[Clock] build start screen=%p\n", screen);
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
        Serial.printf("[Clock] canvas buffer allocated=%p bytes=%u\n",
                      s_canvas_buf, (unsigned)(CANVAS_W * CANVAS_H * sizeof(lv_color_t)));
    }

    s_clock_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_clock_canvas, s_canvas_buf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_clock_canvas, (480 - CANVAS_W) / 2, 20);
    lv_obj_set_style_border_width(s_clock_canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_clock_canvas, 0, LV_PART_MAIN);

    draw_clock_face(0, 0, 0);

    s_lbl_wifi = lv_label_create(screen);
    lv_obj_set_style_text_font(s_lbl_wifi, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_wifi, lv_color_make(220, 40, 40), LV_PART_MAIN);
    lv_label_set_text(s_lbl_wifi, LV_SYMBOL_WIFI);
    lv_obj_align(s_lbl_wifi, LV_ALIGN_TOP_RIGHT, -42, 10);

    s_lbl_ntp = lv_label_create(screen);
    lv_obj_set_style_text_font(s_lbl_ntp, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_ntp, lv_color_make(200, 150, 0), LV_PART_MAIN);
    lv_label_set_text(s_lbl_ntp, LV_SYMBOL_REFRESH);
    lv_obj_align(s_lbl_ntp, LV_ALIGN_TOP_RIGHT, -12, 10);

    s_lbl_time = lv_label_create(screen);
    lv_obj_set_style_text_font(s_lbl_time, &lv_font_montserrat_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_time, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(s_lbl_time, "--:--:--");
    lv_obj_align(s_lbl_time, LV_ALIGN_TOP_MID, 0, 420);

    s_lbl_date = lv_label_create(screen);
    lv_obj_set_style_text_font(s_lbl_date, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_date, lv_color_make(170, 170, 170), LV_PART_MAIN);
    lv_label_set_text(s_lbl_date, "--/--/--");
    lv_obj_align(s_lbl_date, LV_ALIGN_TOP_MID, 0, 454);

    lv_obj_invalidate(screen);
    Serial.printf("[Clock] build done canvas=%p children=%u\n",
                  s_clock_canvas, (unsigned)lv_obj_get_child_cnt(screen));
}

// ─────────────────────────────────────────────────────────────────────────────
// Update clock display (called every second when mode == MODE_CLOCK)
// ─────────────────────────────────────────────────────────────────────────────
void ui_clock_update(struct tm *t, bool synced) {
    if (s_clock_canvas == nullptr || s_lbl_time == nullptr || s_lbl_date == nullptr ||
        s_lbl_wifi == nullptr || s_lbl_ntp == nullptr) {
        Serial.println("[Clock] ERREUR: update avant build complet");
        return;
    }

    int hour = 0;
    int minute = 0;
    int second = 0;
    if (synced && t != nullptr) {
        hour = t->tm_hour;
        minute = t->tm_min;
        second = t->tm_sec;
    }

    draw_clock_face(hour, minute, second);

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
        lv_label_set_text(s_lbl_wifi, LV_SYMBOL_WIFI);
    } else {
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_make(220, 40, 40), LV_PART_MAIN);
        lv_label_set_text(s_lbl_wifi, LV_SYMBOL_WIFI);
    }

    if (synced) {
        lv_obj_set_style_text_color(s_lbl_ntp, lv_color_make(0, 180, 255), LV_PART_MAIN);
        lv_label_set_text(s_lbl_ntp, LV_SYMBOL_REFRESH);
    } else {
        lv_obj_set_style_text_color(s_lbl_ntp, lv_color_make(200, 150, 0), LV_PART_MAIN);
        lv_label_set_text(s_lbl_ntp, LV_SYMBOL_REFRESH);
    }

    lv_obj_align(s_lbl_time, LV_ALIGN_TOP_MID, 0, 420);
    lv_obj_align(s_lbl_date, LV_ALIGN_TOP_MID, 0, 454);

    lv_obj_invalidate(lv_scr_act());
}

// ─────────────────────────────────────────────────────────────────────────────
void ui_clock_refresh_status(bool wifi_ok, const char *ip) {
    if (s_lbl_wifi == nullptr) return;
    (void)ip;
    if (wifi_ok) {
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_make(0, 210, 0), LV_PART_MAIN);
        lv_label_set_text(s_lbl_wifi, LV_SYMBOL_WIFI);
    } else {
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_make(220, 40, 40), LV_PART_MAIN);
        lv_label_set_text(s_lbl_wifi, LV_SYMBOL_WIFI);
    }
}
