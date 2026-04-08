#include "ui_countdown.h"
#include "app_mode.h"
#include <lvgl.h>
#include <Arduino.h>

static lv_obj_t *s_lbl_countdown = nullptr;
static lv_obj_t *s_btn_start     = nullptr;
static lv_obj_t *s_lbl_start     = nullptr;

static uint32_t s_last_tick_ms = 0;

static void log_countdown_press(const char *name) {
    lv_indev_t *indev = lv_indev_get_act();
    if (indev == nullptr) {
        Serial.printf("[Countdown] %s clicked indev=<null>\n", name);
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    Serial.printf("[Countdown] %s clicked point=(%d,%d) remaining=%lu state=%d\n",
                  name, point.x, point.y,
                  (unsigned long)g_countdown_ms, (int)g_cd_state);
}

void ui_countdown_reset_refs() {
    s_lbl_countdown = nullptr;
    s_btn_start = nullptr;
    s_lbl_start = nullptr;
}

void countdown_tick(uint32_t now_ms) {
    if (g_cd_state != CD_RUNNING) {
        s_last_tick_ms = now_ms;
        return;
    }

    uint32_t elapsed = now_ms - s_last_tick_ms;
    s_last_tick_ms = now_ms;

    if (elapsed >= g_countdown_ms) {
        g_countdown_ms = 0;
        g_cd_state = CD_DONE;
    } else {
        g_countdown_ms -= elapsed;
    }
}

static void format_countdown(char *buf, size_t len) {
    uint32_t total_s = g_countdown_ms / 1000;
    uint32_t h = total_s / 3600;
    uint32_t m = (total_s % 3600) / 60;
    uint32_t s = total_s % 60;
    snprintf(buf, len, "%02lu:%02lu:%02lu",
             (unsigned long)h,
             (unsigned long)m,
             (unsigned long)s);
}

static void refresh_start_label() {
    if (s_lbl_start == nullptr) return;
    lv_label_set_text(s_lbl_start, g_cd_state == CD_RUNNING ? "PAUSE" : "START");
}

static void cb_adj(lv_event_t *e) {
    intptr_t delta_ms = (intptr_t)lv_event_get_user_data(e);
    Serial.printf("[Countdown] ADJ clicked delta_ms=%ld\n", (long)delta_ms);
    if (g_cd_state == CD_RUNNING) return;

    if (delta_ms > 0) {
        g_countdown_ms += (uint32_t)delta_ms;
    } else {
        uint32_t sub = (uint32_t)(-delta_ms);
        g_countdown_ms = (g_countdown_ms > sub) ? g_countdown_ms - sub : 0;
    }

    if (s_lbl_countdown != nullptr) {
        char buf[12];
        format_countdown(buf, sizeof(buf));
        lv_label_set_text(s_lbl_countdown, buf);
    }
}

static void cb_start(lv_event_t *e) {
    (void)e;
    log_countdown_press("START");
    if (g_cd_state == CD_DONE) return;
    if (g_cd_state == CD_IDLE && g_countdown_ms == 0) return;
    g_cd_state = (g_cd_state == CD_RUNNING) ? CD_IDLE : CD_RUNNING;
    refresh_start_label();
}

static void cb_reset(lv_event_t *e) {
    (void)e;
    log_countdown_press("RESET");
    g_countdown_ms = 0;
    g_cd_state = CD_IDLE;
    if (s_lbl_countdown != nullptr) lv_label_set_text(s_lbl_countdown, "00:00:00");
    refresh_start_label();
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *text,
                          lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h,
                          lv_event_cb_t cb, void *user_data) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_make(40, 40, 40), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_make(100, 100, 100), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(lbl);
    return btn;
}

void ui_countdown_build() {
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(screen);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_title, lv_color_make(150, 150, 150), LV_PART_MAIN);
    lv_label_set_text(lbl_title, "countdown");
    lv_obj_set_pos(lbl_title, 0, 34);
    lv_obj_set_width(lbl_title, 480);
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    s_lbl_countdown = lv_label_create(screen);
    lv_obj_set_style_text_font(s_lbl_countdown, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_countdown, lv_color_white(), LV_PART_MAIN);
    char buf[12];
    format_countdown(buf, sizeof(buf));
    lv_label_set_text(s_lbl_countdown, buf);
    lv_obj_set_pos(s_lbl_countdown, 0, 86);
    lv_obj_set_width(s_lbl_countdown, 480);
    lv_obj_set_style_text_align(s_lbl_countdown, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    const lv_coord_t col_x[3] = { 18, 166, 314 };
    const lv_coord_t btn_w = 148;
    const lv_coord_t btn_h = 64;

    make_btn(screen, "+1h", col_x[0], 180, btn_w, btn_h, cb_adj, (void *)(3600000));
    make_btn(screen, "+1m", col_x[1], 180, btn_w, btn_h, cb_adj, (void *)(60000));
    make_btn(screen, "+1s", col_x[2], 180, btn_w, btn_h, cb_adj, (void *)(1000));

    make_btn(screen, "-1h", col_x[0], 260, btn_w, btn_h, cb_adj, (void *)(-3600000));
    make_btn(screen, "-1m", col_x[1], 260, btn_w, btn_h, cb_adj, (void *)(-60000));
    make_btn(screen, "-1s", col_x[2], 260, btn_w, btn_h, cb_adj, (void *)(-1000));

    s_btn_start = lv_btn_create(screen);
    lv_obj_set_size(s_btn_start, 212, 72);
    lv_obj_set_pos(s_btn_start, 18, 360);
    lv_obj_set_style_bg_color(s_btn_start, lv_color_make(0, 100, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_btn_start, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_btn_start, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(s_btn_start, cb_start, LV_EVENT_CLICKED, nullptr);

    s_lbl_start = lv_label_create(s_btn_start);
    lv_obj_set_style_text_font(s_lbl_start, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_start, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(s_lbl_start);
    refresh_start_label();

    lv_obj_t *btn_reset = lv_btn_create(screen);
    lv_obj_set_size(btn_reset, 212, 72);
    lv_obj_set_pos(btn_reset, 250, 360);
    lv_obj_set_style_bg_color(btn_reset, lv_color_make(100, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn_reset, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn_reset, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_reset, cb_reset, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "RESET");
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_reset, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(lbl_reset);
}

void ui_countdown_update() {
    if (s_lbl_countdown == nullptr) return;
    char buf[12];
    format_countdown(buf, sizeof(buf));
    lv_label_set_text(s_lbl_countdown, buf);
    refresh_start_label();
}
