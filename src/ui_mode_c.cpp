#include "ui_mode_c.h"
#include <lvgl.h>
#include <Arduino.h>
#include <math.h>
#include "secrets.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

extern void buzzer_request_navigation_tick(uint32_t now_ms);

#ifdef MODE_C_ITEMS
static const char * const s_wheel_items[] = { MODE_C_ITEMS };
#else
static const char * const s_wheel_items[] = {
    "Option 1",
    "Option 2",
    "Option 3",
    "Option 4",
};
#endif

static constexpr uint16_t WHEEL_SIZE = 360;
static constexpr lv_coord_t POINTER_Y = 52;
static constexpr uint32_t SPIN_DURATION_MS = 4200;
static constexpr lv_coord_t WHEEL_RADIUS = 156;
static constexpr lv_coord_t WHEEL_INNER_RADIUS = 48;
static constexpr size_t WHEEL_ITEM_COUNT = sizeof(s_wheel_items) / sizeof(s_wheel_items[0]);

static lv_obj_t *s_wheel_wrap = nullptr;
static lv_obj_t *s_result_label = nullptr;
static lv_obj_t *s_spin_label = nullptr;
static lv_obj_t *s_spin_button = nullptr;
static lv_obj_t *s_segments[WHEEL_ITEM_COUNT] = { nullptr };
static lv_obj_t *s_separators[WHEEL_ITEM_COUNT] = { nullptr };
static lv_obj_t *s_labels[WHEEL_ITEM_COUNT] = { nullptr };

static bool s_spinning = false;
static uint32_t s_spin_start_ms = 0;
static uint32_t s_spin_duration_ms = SPIN_DURATION_MS;
static uint32_t s_start_angle_tenths = 0;
static uint32_t s_target_angle_tenths = 0;
static uint16_t s_wheel_angle_tenths = 0;
static int s_selected_index = 0;
static int s_last_reported_index = -1;
static int s_last_tick_index = -1;
static lv_point_t s_separator_points[WHEEL_ITEM_COUNT][2];

static int normalize_degrees(int degrees) {
    int result = degrees % 360;
    return result < 0 ? result + 360 : result;
}

static int circular_distance_degrees(int a, int b) {
    int diff = abs(normalize_degrees(a) - normalize_degrees(b));
    return diff > 180 ? 360 - diff : diff;
}

static lv_color_t wheel_palette(size_t index) {
    static const lv_color_t colors[] = {
        lv_color_make(236, 236, 236),
        lv_color_make(210, 210, 210),
        lv_color_make(190, 30, 35),
        lv_color_make(154, 154, 154),
        lv_color_make(248, 248, 248),
        lv_color_make(128, 24, 28),
        lv_color_make(182, 182, 182),
        lv_color_make(106, 106, 106),
    };
    return colors[index % (sizeof(colors) / sizeof(colors[0]))];
}

static size_t wheel_item_count() {
    return WHEEL_ITEM_COUNT;
}

static uint16_t current_wheel_angle() {
    return s_wheel_angle_tenths;
}

static float ease_out_cubic(float t) {
    float inv = 1.0f - t;
    return 1.0f - (inv * inv * inv);
}

static void set_wheel_angle(uint16_t angle_tenths) {
    s_wheel_angle_tenths = angle_tenths % 3600U;

    if (s_wheel_wrap == nullptr) return;

    const int center = WHEEL_SIZE / 2;
    const int step_deg = 360 / (int)WHEEL_ITEM_COUNT;
    const int rotation_deg = (int)(s_wheel_angle_tenths / 10U);
    const int label_radius = 108;

    for (size_t index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        const int base_start_deg = 270 + ((int)index * step_deg);
        const int start_deg = normalize_degrees(base_start_deg + rotation_deg);
        const int end_deg = normalize_degrees(start_deg + step_deg);
        const int mid_deg = normalize_degrees(base_start_deg + rotation_deg + (step_deg / 2));
        const float boundary_rad = (float)start_deg * ((float)M_PI / 180.0f);
        const float mid_rad = (float)mid_deg * ((float)M_PI / 180.0f);

        if (s_segments[index] != nullptr) {
            lv_arc_set_bg_angles(s_segments[index], start_deg, end_deg);
        }

        if (s_separators[index] != nullptr) {
            s_separator_points[index][0].x = (lv_coord_t)(center + (int)(cosf(boundary_rad) * WHEEL_INNER_RADIUS));
            s_separator_points[index][0].y = (lv_coord_t)(center + (int)(sinf(boundary_rad) * WHEEL_INNER_RADIUS));
            s_separator_points[index][1].x = (lv_coord_t)(center + (int)(cosf(boundary_rad) * WHEEL_RADIUS));
            s_separator_points[index][1].y = (lv_coord_t)(center + (int)(sinf(boundary_rad) * WHEEL_RADIUS));
            lv_line_set_points(s_separators[index], s_separator_points[index], 2);
        }

        if (s_labels[index] != nullptr) {
            lv_obj_align(s_labels[index],
                         LV_ALIGN_TOP_LEFT,
                         center + (int)(cosf(mid_rad) * label_radius) - 59,
                         center + (int)(sinf(mid_rad) * label_radius) - 14);
        }
    }
}

static void create_segment(size_t index, size_t total) {
    if (s_wheel_wrap == nullptr || total == 0) return;

    const int center = WHEEL_SIZE / 2;
    const int step_deg = 360 / (int)total;
    const int start_deg = 270 + ((int)index * step_deg);
    const int end_deg = start_deg + step_deg;
    const int mid_deg = start_deg + (step_deg / 2);
    const float mid_rad = (float)mid_deg * ((float)M_PI / 180.0f);
    const int label_radius = 108;

    lv_obj_t *segment = lv_arc_create(s_wheel_wrap);
    s_segments[index] = segment;
    lv_obj_remove_style_all(segment);
    lv_obj_set_size(segment, WHEEL_SIZE, WHEEL_SIZE);
    lv_obj_center(segment);
    lv_arc_set_rotation(segment, 0);
    lv_arc_set_bg_angles(segment, start_deg, end_deg);
    lv_obj_clear_flag(segment, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(segment, wheel_palette(index), LV_PART_MAIN);
    lv_obj_set_style_arc_width(segment, WHEEL_RADIUS - WHEEL_INNER_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(segment, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(segment, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(segment, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(segment, LV_OPA_TRANSP, LV_PART_KNOB);

    lv_obj_t *separator = lv_line_create(s_wheel_wrap);
    s_separators[index] = separator;
    const float boundary_rad = (float)start_deg * ((float)M_PI / 180.0f);
    lv_point_t *points = s_separator_points[index];
    points[0].x = (lv_coord_t)(center + (int)(cosf(boundary_rad) * WHEEL_INNER_RADIUS));
    points[0].y = (lv_coord_t)(center + (int)(sinf(boundary_rad) * WHEEL_INNER_RADIUS));
    points[1].x = (lv_coord_t)(center + (int)(cosf(boundary_rad) * WHEEL_RADIUS));
    points[1].y = (lv_coord_t)(center + (int)(sinf(boundary_rad) * WHEEL_RADIUS));
    lv_line_set_points(separator, points, 2);
    lv_obj_set_style_line_color(separator, lv_color_make(10, 10, 10), LV_PART_MAIN);
    lv_obj_set_style_line_width(separator, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(separator, false, LV_PART_MAIN);
    lv_obj_clear_flag(separator, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(s_wheel_wrap);
    s_labels[index] = label;
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(label,
                                index % 3 == 2 ? lv_color_white() : lv_color_make(18, 18, 18),
                                LV_PART_MAIN);
    lv_label_set_text(label, s_wheel_items[index]);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, 118);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label,
                 LV_ALIGN_TOP_LEFT,
                 center + (int)(cosf(mid_rad) * label_radius) - 59,
                 center + (int)(sinf(mid_rad) * label_radius) - 14);
}

static void build_wheel() {
    if (s_wheel_wrap == nullptr) return;

    size_t total = wheel_item_count();
    for (size_t index = 0; index < total; ++index) {
        create_segment(index, total);
    }

    lv_obj_t *outer_ring = lv_arc_create(s_wheel_wrap);
    lv_obj_remove_style_all(outer_ring);
    lv_obj_set_size(outer_ring, WHEEL_SIZE, WHEEL_SIZE);
    lv_obj_center(outer_ring);
    lv_arc_set_bg_angles(outer_ring, 0, 360);
    lv_obj_set_style_arc_color(outer_ring, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(outer_ring, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(outer_ring, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(outer_ring, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(outer_ring, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(outer_ring, LV_OPA_TRANSP, LV_PART_KNOB);

    lv_obj_t *hub = lv_obj_create(s_wheel_wrap);
    lv_obj_set_size(hub, 94, 94);
    lv_obj_center(hub);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hub, lv_color_make(22, 22, 22), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(hub, 0, LV_PART_MAIN);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *cap = lv_obj_create(s_wheel_wrap);
    lv_obj_set_size(cap, 34, 34);
    lv_obj_center(cap);
    lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cap, lv_color_make(190, 30, 35), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(cap, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(cap, 4, LV_PART_MAIN);
    lv_obj_clear_flag(cap, LV_OBJ_FLAG_CLICKABLE);
}

static void build_pointer(lv_obj_t *screen) {
    lv_obj_t *pointer = lv_obj_create(screen);
    lv_obj_remove_style_all(pointer);
    lv_obj_set_size(pointer, 34, 20);
    lv_obj_align(pointer, LV_ALIGN_TOP_MID, 0, POINTER_Y);
    lv_obj_set_style_bg_color(pointer, lv_color_make(190, 30, 35), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pointer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(pointer, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(pointer, 0, LV_PART_MAIN);

    static lv_point_t points[] = {
        { 0, 0 },
        { -16, -18 },
        { 16, -18 },
    };
    lv_obj_t *tip = lv_line_create(screen);
    lv_line_set_points(tip, points, 3);
    lv_obj_set_style_line_color(tip, lv_color_make(190, 30, 35), LV_PART_MAIN);
    lv_obj_set_style_line_width(tip, 6, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(tip, false, LV_PART_MAIN);
    lv_obj_align(tip, LV_ALIGN_TOP_MID, 0, POINTER_Y + 18);
}

static int winner_from_angle(uint16_t angle_tenths) {
    const size_t total = wheel_item_count();
    if (total == 0) return 0;

    const int step_deg = 360 / (int)total;
    const int rotation_deg = (int)(angle_tenths / 10U);
    const int pointer_deg = 270;

    int best_index = 0;
    int best_distance = 361;
    for (size_t index = 0; index < total; ++index) {
        int center_deg = 270 + ((int)index * step_deg) + (step_deg / 2) + rotation_deg;
        int distance = circular_distance_degrees(center_deg, pointer_deg);
        if (distance < best_distance) {
            best_distance = distance;
            best_index = (int)index;
        }
    }

    return best_index;
}

static void refresh_result() {
    if (s_result_label == nullptr) return;

    int visible_index = winner_from_angle(current_wheel_angle());
    s_selected_index = visible_index;

    if (visible_index != s_last_reported_index || !s_spinning) {
        lv_label_set_text_fmt(s_result_label, "%s", s_wheel_items[visible_index]);
        s_last_reported_index = visible_index;
    }

    if (s_spin_label != nullptr) {
        lv_label_set_text(s_spin_label, s_spinning ? "SPINNING..." : "SPIN");
    }
}

static void start_spin() {
    const size_t total = wheel_item_count();
    if (total < 2) {
        if (s_result_label != nullptr) {
            lv_label_set_text(s_result_label, s_wheel_items[0]);
        }
        return;
    }

    uint32_t now = millis();
    uint32_t random_turns = 5 + (esp_random() % 4);
    size_t target_index = esp_random() % total;
    int32_t slice = 3600 / (int32_t)total;
    int32_t base_center = 2700 + ((int32_t)target_index * slice) + (slice / 2);
    int32_t landing_offset = 2700 - base_center;
    while (landing_offset < 0) {
        landing_offset += 3600;
    }

    s_spinning = true;
    s_spin_start_ms = now;
    s_spin_duration_ms = SPIN_DURATION_MS + (esp_random() % 900);
    s_start_angle_tenths = current_wheel_angle();
    s_target_angle_tenths = s_start_angle_tenths + (random_turns * 3600UL) + (uint32_t)landing_offset;
    s_last_tick_index = winner_from_angle((uint16_t)(s_start_angle_tenths % 3600U));

    s_selected_index = (int)target_index;
    s_last_reported_index = -1;
    refresh_result();
    Serial.printf("[Mode C] spin start target=%d duration=%lu angle=%u->%u\n",
                  s_selected_index,
                  (unsigned long)s_spin_duration_ms,
                  s_start_angle_tenths,
                  (unsigned)(s_target_angle_tenths % 3600UL));
}

static void spin_button_cb(lv_event_t *event) {
    (void)event;
    if (s_spinning) return;
    start_spin();
}

void ui_mode_c_reset_refs() {
    s_wheel_wrap = nullptr;
    s_result_label = nullptr;
    s_spin_label = nullptr;
    s_spin_button = nullptr;
    s_last_reported_index = -1;
    s_last_tick_index = -1;
    s_wheel_angle_tenths = 0;
    for (size_t index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        s_segments[index] = nullptr;
        s_separators[index] = nullptr;
        s_labels[index] = nullptr;
    }
}

void ui_mode_c_build() {
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_make(10, 10, 10), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(screen, lv_color_make(34, 34, 34), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_make(232, 232, 232), LV_PART_MAIN);
    lv_label_set_text(title, "Wheel");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    build_pointer(screen);

    s_wheel_wrap = lv_obj_create(screen);
    lv_obj_remove_style_all(s_wheel_wrap);
    lv_obj_set_size(s_wheel_wrap, WHEEL_SIZE, WHEEL_SIZE);
    lv_obj_align(s_wheel_wrap, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_style_bg_opa(s_wheel_wrap, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_wheel_wrap, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_wheel_wrap, LV_OBJ_FLAG_SCROLLABLE);
    build_wheel();
    set_wheel_angle(0);

    lv_obj_t *result_panel = lv_obj_create(screen);
    lv_obj_set_size(result_panel, 420, 70);
    lv_obj_align(result_panel, LV_ALIGN_BOTTOM_MID, 0, -92);
    lv_obj_set_style_bg_color(result_panel, lv_color_make(20, 20, 24), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(result_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(result_panel, lv_color_make(235, 235, 235), LV_PART_MAIN);
    lv_obj_set_style_border_width(result_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(result_panel, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_all(result_panel, 12, LV_PART_MAIN);

    s_result_label = lv_label_create(result_panel);
    lv_obj_set_style_text_font(s_result_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_result_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_long_mode(s_result_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_result_label, 390);
    lv_obj_center(s_result_label);

    s_spin_button = lv_btn_create(screen);
    lv_obj_set_size(s_spin_button, 248, 62);
    lv_obj_align(s_spin_button, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_bg_color(s_spin_button, lv_color_make(190, 30, 35), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(s_spin_button, lv_color_make(126, 18, 22), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(s_spin_button, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_spin_button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_spin_button, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_spin_button, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_spin_button, 18, LV_PART_MAIN);
    lv_obj_add_event_cb(s_spin_button, spin_button_cb, LV_EVENT_CLICKED, nullptr);

    s_spin_label = lv_label_create(s_spin_button);
    lv_obj_set_style_text_font(s_spin_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_spin_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(s_spin_label);

    if (s_selected_index >= (int)wheel_item_count()) {
        s_selected_index = 0;
    }
    s_spinning = false;
    refresh_result();
}

void ui_mode_c_tick(uint32_t now_ms) {
    if (s_wheel_wrap == nullptr) return;

    if (!s_spinning) {
        refresh_result();
        return;
    }

    uint32_t elapsed = now_ms - s_spin_start_ms;
    if (elapsed >= s_spin_duration_ms) {
        s_spinning = false;
        set_wheel_angle((uint16_t)(s_target_angle_tenths % 3600UL));
        s_selected_index = winner_from_angle(current_wheel_angle());
        s_last_tick_index = s_selected_index;
        refresh_result();
        Serial.printf("[Mode C] spin end winner=%s angle=%u\n",
                      s_wheel_items[s_selected_index],
                      (unsigned)(s_target_angle_tenths % 3600UL));
        return;
    }

    float progress = (float)elapsed / (float)s_spin_duration_ms;
    float eased = ease_out_cubic(progress);
    float angle = (float)s_start_angle_tenths + ((float)(s_target_angle_tenths - s_start_angle_tenths) * eased);
    uint16_t normalized_angle = (uint16_t)fmodf(angle, 3600.0f);
    set_wheel_angle(normalized_angle);

    int visible_index = winner_from_angle(normalized_angle);
    if (visible_index != s_last_tick_index) {
        buzzer_request_navigation_tick(now_ms);
        s_last_tick_index = visible_index;
    }

    refresh_result();
}