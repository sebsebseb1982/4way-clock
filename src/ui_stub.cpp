#include "ui_stub.h"
#include <lvgl.h>

void ui_stub_build(const char *label) {
    lv_obj_t *screen = lv_scr_act();

    lv_obj_set_style_bg_color(screen, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 380, 220);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_make(18, 18, 18), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_make(90, 90, 90), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 24, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(panel);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_make(230, 230, 230), LV_PART_MAIN);
    lv_label_set_text_fmt(lbl, "%s\n\nA definir", label);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}
