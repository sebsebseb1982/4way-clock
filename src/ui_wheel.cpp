#include "ui_wheel.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>

#include "secrets.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

extern void buzzer_request_navigation_tick(uint32_t now_ms);

#ifdef WHEEL_ITEMS
static const char * const s_wheel_items[] = { WHEEL_ITEMS };
#else
static const char * const s_wheel_items[] = {
    "Option 1",
    "Option 2",
    "Option 3",
    "Option 4",
};
#endif

static constexpr size_t WHEEL_ITEM_COUNT = sizeof(s_wheel_items) / sizeof(s_wheel_items[0]);
static constexpr size_t WHEEL_MAX_ITEM_CHARS = 18;
static constexpr lv_coord_t WHEEL_CANVAS_SIZE = 332;
static constexpr lv_coord_t WHEEL_CANVAS_TOP = 78;
static constexpr lv_coord_t WHEEL_CENTER_X = 240;
static constexpr lv_coord_t WHEEL_CENTER_Y = WHEEL_CANVAS_TOP + (WHEEL_CANVAS_SIZE / 2);
static constexpr lv_coord_t WHEEL_OUTER_RADIUS = 148;
static constexpr lv_coord_t WHEEL_INNER_RADIUS = 42;
static constexpr lv_coord_t WHEEL_TEXT_RADIUS = 104;
static constexpr lv_coord_t WHEEL_CHAR_CANVAS_SIZE = 28;
static constexpr uint32_t SPIN_DURATION_MS = 4200;
static constexpr uint32_t FRAME_INTERVAL_MS = 20;
static constexpr size_t WHEEL_BUF_SIZE = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(WHEEL_CANVAS_SIZE, WHEEL_CANVAS_SIZE);
static constexpr size_t WHEEL_CHAR_BUF_SIZE = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(WHEEL_CHAR_CANVAS_SIZE, WHEEL_CHAR_CANVAS_SIZE);
static constexpr lv_coord_t POINTER_CANVAS_W = 36;
static constexpr lv_coord_t POINTER_CANVAS_H = 28;
static constexpr size_t POINTER_BUF_SIZE = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(POINTER_CANVAS_W, POINTER_CANVAS_H);

static uint8_t *s_canvas_buf = nullptr;
static uint8_t *s_pointer_buf = nullptr;
static lv_obj_t *s_wheel_canvas = nullptr;
static lv_obj_t *s_wheel_outline = nullptr;
static lv_obj_t *s_result_label = nullptr;
static lv_obj_t *s_spin_label = nullptr;
static lv_obj_t *s_pointer_canvas = nullptr;
static lv_obj_t *s_char_canvases[WHEEL_ITEM_COUNT][WHEEL_MAX_ITEM_CHARS] = { nullptr };
static uint8_t *s_char_canvas_bufs[WHEEL_ITEM_COUNT][WHEEL_MAX_ITEM_CHARS] = { nullptr };
static char s_char_text[WHEEL_ITEM_COUNT][WHEEL_MAX_ITEM_CHARS][2] = {{{0}}};
static uint8_t s_item_char_count[WHEEL_ITEM_COUNT] = { 0 };

static bool s_spinning = false;
static uint32_t s_spin_start_ms = 0;
static uint32_t s_spin_duration_ms = SPIN_DURATION_MS;
static uint32_t s_start_angle_tenths = 0;
static uint32_t s_target_angle_tenths = 0;
static uint32_t s_last_anim_frame_ms = 0;
static uint16_t s_wheel_angle_tenths = 0;
static int s_selected_index = 0;
static int s_last_reported_index = -1;
static int s_last_tick_index = -1;

static lv_color_t wheel_palette(size_t index) {
    static const lv_color_t colors[] = {
        lv_color_make(238, 238, 238),
        lv_color_make(206, 206, 206),
        lv_color_make(189, 29, 34),
        lv_color_make(136, 136, 136),
        lv_color_make(248, 248, 248),
        lv_color_make(120, 22, 26),
        lv_color_make(176, 176, 176),
        lv_color_make(96, 96, 96),
    };
    return colors[index % (sizeof(colors) / sizeof(colors[0]))];
}

static int32_t normalize_tenths(int32_t angle_tenths) {
    int32_t result = angle_tenths % 3600;
    return result < 0 ? result + 3600 : result;
}

static uint16_t current_wheel_angle() {
    return s_wheel_angle_tenths;
}

static float ease_out_cubic(float t) {
    float inv = 1.0f - t;
    return 1.0f - (inv * inv * inv);
}

static lv_point_t wheel_point(int center, int radius, float degrees) {
    float radians = degrees * ((float)M_PI / 180.0f);
    lv_point_t point = {
        (lv_coord_t)(center + (int)roundf(cosf(radians) * radius)),
        (lv_coord_t)(center + (int)roundf(sinf(radians) * radius)),
    };
    return point;
}

static void draw_wheel_face() {
    if (s_wheel_canvas == nullptr) return;

    const int center = WHEEL_CANVAS_SIZE / 2;
    const float slice_deg = 360.0f / (float)WHEEL_ITEM_COUNT;
    const int steps_per_slice = 20;

    lv_canvas_fill_bg(s_wheel_canvas, lv_color_make(0, 0, 0), LV_OPA_TRANSP);

    for (size_t index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        float start_deg = -90.0f + (slice_deg * (float)index);
        float end_deg = start_deg + slice_deg;

        lv_point_t points[24];
        uint32_t point_count = 0;

        points[point_count++] = { (lv_coord_t)center, (lv_coord_t)center };

        for (int step = 0; step <= steps_per_slice; ++step) {
            float angle = start_deg + ((end_deg - start_deg) * (float)step / (float)steps_per_slice);
            points[point_count++] = wheel_point(center, WHEEL_OUTER_RADIUS, angle);
        }

        lv_draw_rect_dsc_t sector_dsc;
        lv_draw_rect_dsc_init(&sector_dsc);
        sector_dsc.bg_color = wheel_palette(index);
        sector_dsc.bg_opa = LV_OPA_COVER;
        sector_dsc.border_color = wheel_palette(index);
        sector_dsc.border_opa = LV_OPA_COVER;
        sector_dsc.border_width = 1;
        sector_dsc.radius = 0;
        lv_canvas_draw_polygon(s_wheel_canvas, points, point_count, &sector_dsc);
    }

    lv_draw_arc_dsc_t inner_ring;
    lv_draw_arc_dsc_init(&inner_ring);
    inner_ring.color = lv_color_make(14, 14, 14);
    inner_ring.width = (WHEEL_INNER_RADIUS * 2) + 14;
    inner_ring.opa = LV_OPA_COVER;
    inner_ring.rounded = 0;
    lv_canvas_draw_arc(s_wheel_canvas, center, center, 0, 0, 360, &inner_ring);

    lv_draw_rect_dsc_t cap_dsc;
    lv_draw_rect_dsc_init(&cap_dsc);
    cap_dsc.bg_color = lv_color_make(189, 29, 34);
    cap_dsc.bg_opa = LV_OPA_COVER;
    cap_dsc.radius = LV_RADIUS_CIRCLE;
    cap_dsc.border_color = lv_color_white();
    cap_dsc.border_width = 4;
    lv_canvas_draw_rect(s_wheel_canvas, center - 16, center - 16, 32, 32, &cap_dsc);
}

static bool use_light_text(size_t index) {
    return (index % 3) == 2 || (index % 8) == 7;
}

static void update_label_layout() {
    const float slice_tenths = 3600.0f / (float)WHEEL_ITEM_COUNT;

    for (size_t index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        uint8_t char_count = s_item_char_count[index];
        if (char_count == 0) continue;

        float item_center_tenths = 2700.0f + ((float)index * slice_tenths) + (slice_tenths / 2.0f) + (float)s_wheel_angle_tenths;
        float max_span = slice_tenths * 0.72f;
        float base_spacing = 78.0f;
        float total_span = (char_count > 1) ? base_spacing * (float)(char_count - 1) : 0.0f;
        if (total_span > max_span && char_count > 1) {
            base_spacing = max_span / (float)(char_count - 1);
            total_span = max_span;
        }
        float start_offset = -total_span / 2.0f;

        for (uint8_t char_index = 0; char_index < char_count; ++char_index) {
            lv_obj_t *glyph = s_char_canvases[index][char_index];
            if (glyph == nullptr) continue;

            float char_tenths = item_center_tenths + start_offset + ((float)char_index * base_spacing);
            float radians = (char_tenths / 10.0f) * ((float)M_PI / 180.0f);
            int tangent_tenths = (int)normalize_tenths((int32_t)char_tenths + 900);

            lv_obj_set_pos(glyph,
                           WHEEL_CENTER_X + (int)(cosf(radians) * WHEEL_TEXT_RADIUS) - (WHEEL_CHAR_CANVAS_SIZE / 2),
                           WHEEL_CENTER_Y + (int)(sinf(radians) * WHEEL_TEXT_RADIUS) - (WHEEL_CHAR_CANVAS_SIZE / 2));
            lv_img_set_angle(glyph, tangent_tenths);
        }
    }
}

static void set_wheel_angle(uint16_t angle_tenths) {
    s_wheel_angle_tenths = angle_tenths % 3600U;
    if (s_wheel_canvas != nullptr) {
        lv_img_set_angle(s_wheel_canvas, s_wheel_angle_tenths);
    }
    update_label_layout();
}

static int winner_from_angle(uint16_t angle_tenths) {
    if (WHEEL_ITEM_COUNT == 0) return 0;

    float slice_tenths = 3600.0f / (float)WHEEL_ITEM_COUNT;
    float pointer_on_wheel = (float)normalize_tenths(2700 - (int32_t)angle_tenths);
    float relative = (float)normalize_tenths((int32_t)pointer_on_wheel - 2700);
    int index = (int)floorf(relative / slice_tenths);
    if (index < 0) index = 0;
    if (index >= (int)WHEEL_ITEM_COUNT) index = (int)WHEEL_ITEM_COUNT - 1;
    return index;
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
        lv_label_set_text(s_spin_label, s_spinning ? "spinning..." : "spin");
    }
}

static void start_spin() {
    if (WHEEL_ITEM_COUNT < 2) {
        if (s_result_label != nullptr) {
            lv_label_set_text(s_result_label, s_wheel_items[0]);
        }
        return;
    }

    uint32_t now = millis();
    uint32_t random_turns = 5 + (esp_random() % 4);
    uint32_t random_stop = esp_random() % 3600U;

    s_spinning = true;
    s_spin_start_ms = now;
    s_spin_duration_ms = SPIN_DURATION_MS + (esp_random() % 900U);
    s_start_angle_tenths = current_wheel_angle();
    s_target_angle_tenths = s_start_angle_tenths + (random_turns * 3600UL) + random_stop;
    s_last_anim_frame_ms = 0;
    s_last_tick_index = winner_from_angle((uint16_t)(s_start_angle_tenths % 3600U));
    s_last_reported_index = -1;
    refresh_result();

    Serial.printf("[Wheel] spin start duration=%lu angle=%u->%u\n",
                  (unsigned long)s_spin_duration_ms,
                  (unsigned)s_start_angle_tenths,
                  (unsigned)(s_target_angle_tenths % 3600UL));
}

static void spin_button_cb(lv_event_t *event) {
    (void)event;
    if (s_spinning) return;
    start_spin();
}

static void build_pointer(lv_obj_t *screen) {
    if (s_pointer_buf == nullptr) {
        s_pointer_buf = (uint8_t *)heap_caps_malloc(POINTER_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_pointer_buf == nullptr) {
            s_pointer_buf = (uint8_t *)malloc(POINTER_BUF_SIZE);
        }
        if (s_pointer_buf == nullptr) return;
        memset(s_pointer_buf, 0x00, POINTER_BUF_SIZE);
    }

    s_pointer_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_pointer_canvas, s_pointer_buf, POINTER_CANVAS_W, POINTER_CANVAS_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_set_style_border_width(s_pointer_canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_pointer_canvas, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(s_pointer_canvas, LV_ALIGN_TOP_MID, 0, 58);
    lv_canvas_fill_bg(s_pointer_canvas, lv_color_make(0, 0, 0), LV_OPA_TRANSP);

    lv_point_t points[3] = {
        { POINTER_CANVAS_W / 2, POINTER_CANVAS_H - 1 },
        { 4, 4 },
        { POINTER_CANVAS_W - 4, 4 },
    };
    lv_draw_rect_dsc_t triangle_dsc;
    lv_draw_rect_dsc_init(&triangle_dsc);
    triangle_dsc.bg_color = lv_color_make(189, 29, 34);
    triangle_dsc.bg_opa = LV_OPA_COVER;
    triangle_dsc.border_color = lv_color_make(189, 29, 34);
    triangle_dsc.border_opa = LV_OPA_COVER;
    triangle_dsc.border_width = 1;
    triangle_dsc.radius = 0;
    lv_canvas_draw_polygon(s_pointer_canvas, points, 3, &triangle_dsc);
}

void ui_wheel_reset_refs() {
    s_wheel_canvas = nullptr;
    s_wheel_outline = nullptr;
    s_result_label = nullptr;
    s_spin_label = nullptr;
    s_pointer_canvas = nullptr;
    s_last_reported_index = -1;
    s_last_tick_index = -1;
    s_wheel_angle_tenths = 0;
    s_last_anim_frame_ms = 0;
    for (size_t index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        s_item_char_count[index] = 0;
        for (size_t char_index = 0; char_index < WHEEL_MAX_ITEM_CHARS; ++char_index) {
            s_char_canvases[index][char_index] = nullptr;
            s_char_text[index][char_index][0] = '\0';
            s_char_text[index][char_index][1] = '\0';
        }
    }
}

void ui_wheel_build() {
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_make(10, 10, 10), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(screen, lv_color_make(34, 34, 34), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_make(150, 150, 150), LV_PART_MAIN);
    lv_label_set_text(title, "wheel");
    lv_obj_set_pos(title, 0, 34);
    lv_obj_set_width(title, 480);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    build_pointer(screen);

    if (s_canvas_buf == nullptr) {
        s_canvas_buf = (uint8_t *)heap_caps_malloc(WHEEL_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_canvas_buf == nullptr) {
            s_canvas_buf = (uint8_t *)malloc(WHEEL_BUF_SIZE);
        }
        if (s_canvas_buf == nullptr) {
            Serial.println("[Wheel] ERREUR: allocation canvas impossible");
            return;
        }
        memset(s_canvas_buf, 0x00, WHEEL_BUF_SIZE);
    }

    s_wheel_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_wheel_canvas, s_canvas_buf, WHEEL_CANVAS_SIZE, WHEEL_CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_set_style_border_width(s_wheel_canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_wheel_canvas, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(s_wheel_canvas, LV_ALIGN_TOP_MID, 0, WHEEL_CANVAS_TOP);
    lv_img_set_pivot(s_wheel_canvas, WHEEL_CANVAS_SIZE / 2, WHEEL_CANVAS_SIZE / 2);
    draw_wheel_face();

    s_wheel_outline = lv_arc_create(screen);
    lv_obj_remove_style_all(s_wheel_outline);
    lv_obj_set_size(s_wheel_outline, WHEEL_OUTER_RADIUS * 2 + 12, WHEEL_OUTER_RADIUS * 2 + 12);
    lv_obj_align_to(s_wheel_outline, s_wheel_canvas, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_bg_angles(s_wheel_outline, 0, 360);
    lv_obj_set_style_arc_color(s_wheel_outline, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_wheel_outline, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_wheel_outline, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_wheel_outline, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_wheel_outline, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_wheel_outline, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(s_wheel_outline, LV_OBJ_FLAG_CLICKABLE);

    for (size_t index = 0; index < WHEEL_ITEM_COUNT; ++index) {
        size_t len = strlen(s_wheel_items[index]);
        if (len > WHEEL_MAX_ITEM_CHARS) {
            len = WHEEL_MAX_ITEM_CHARS;
        }
        s_item_char_count[index] = (uint8_t)len;

        for (size_t char_index = 0; char_index < len; ++char_index) {
            s_char_text[index][char_index][0] = s_wheel_items[index][char_index];
            s_char_text[index][char_index][1] = '\0';

            if (s_char_canvas_bufs[index][char_index] == nullptr) {
                s_char_canvas_bufs[index][char_index] = (uint8_t *)heap_caps_malloc(WHEEL_CHAR_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (s_char_canvas_bufs[index][char_index] == nullptr) {
                    s_char_canvas_bufs[index][char_index] = (uint8_t *)malloc(WHEEL_CHAR_BUF_SIZE);
                }
            }
            if (s_char_canvas_bufs[index][char_index] == nullptr) {
                continue;
            }

            s_char_canvases[index][char_index] = lv_canvas_create(screen);
            lv_canvas_set_buffer(s_char_canvases[index][char_index],
                                 s_char_canvas_bufs[index][char_index],
                                 WHEEL_CHAR_CANVAS_SIZE,
                                 WHEEL_CHAR_CANVAS_SIZE,
                                 LV_IMG_CF_TRUE_COLOR_ALPHA);
            lv_obj_set_style_border_width(s_char_canvases[index][char_index], 0, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_char_canvases[index][char_index], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_img_set_pivot(s_char_canvases[index][char_index], WHEEL_CHAR_CANVAS_SIZE / 2, WHEEL_CHAR_CANVAS_SIZE / 2);
            lv_canvas_fill_bg(s_char_canvases[index][char_index], lv_color_make(0, 0, 0), LV_OPA_TRANSP);

            lv_draw_label_dsc_t label_dsc;
            lv_draw_label_dsc_init(&label_dsc);
            label_dsc.font = &lv_font_montserrat_18;
            label_dsc.color = use_light_text(index) ? lv_color_white() : lv_color_make(232, 232, 232);

            lv_point_t text_size;
            lv_txt_get_size(&text_size,
                            s_char_text[index][char_index],
                            label_dsc.font,
                            0,
                            0,
                            LV_COORD_MAX,
                            LV_TEXT_FLAG_NONE);

            lv_canvas_draw_text(s_char_canvases[index][char_index],
                                (WHEEL_CHAR_CANVAS_SIZE - text_size.x) / 2,
                                (WHEEL_CHAR_CANVAS_SIZE - text_size.y) / 2,
                                text_size.x + 2,
                                &label_dsc,
                                s_char_text[index][char_index]);
        }
    }

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

    lv_obj_t *spin_button = lv_btn_create(screen);
    lv_obj_set_size(spin_button, 248, 62);
    lv_obj_align(spin_button, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_bg_color(spin_button, lv_color_make(189, 29, 34), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(spin_button, lv_color_make(126, 18, 22), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(spin_button, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(spin_button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(spin_button, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(spin_button, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(spin_button, 18, LV_PART_MAIN);
    lv_obj_add_event_cb(spin_button, spin_button_cb, LV_EVENT_CLICKED, nullptr);

    s_spin_label = lv_label_create(spin_button);
    lv_obj_set_style_text_font(s_spin_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_spin_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(s_spin_label);

    if (s_selected_index >= (int)WHEEL_ITEM_COUNT) {
        s_selected_index = 0;
    }

    s_spinning = false;
    set_wheel_angle(0);
    refresh_result();
    if (s_pointer_canvas != nullptr) {
        lv_obj_move_foreground(s_pointer_canvas);
    }
}

void ui_wheel_tick(uint32_t now_ms) {
    if (s_wheel_canvas == nullptr) return;

    if (!s_spinning) {
        refresh_result();
        return;
    }

    if (s_last_anim_frame_ms != 0 && (now_ms - s_last_anim_frame_ms) < FRAME_INTERVAL_MS) {
        return;
    }
    s_last_anim_frame_ms = now_ms;

    uint32_t elapsed = now_ms - s_spin_start_ms;
    if (elapsed >= s_spin_duration_ms) {
        s_spinning = false;
        set_wheel_angle((uint16_t)(s_target_angle_tenths % 3600UL));
        s_selected_index = winner_from_angle(current_wheel_angle());
        s_last_tick_index = s_selected_index;
        refresh_result();
        Serial.printf("[Wheel] spin end winner=%s angle=%u\n",
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