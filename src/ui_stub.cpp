#include "ui_stub.h"
#include <lvgl.h>

void ui_stub_build(const char *label) {
    lv_obj_t *screen = lv_scr_act();

    lv_obj_set_style_bg_color(screen, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(screen);
    lv_obj_set_style_text_color(lbl, lv_color_make(100, 100, 100), LV_PART_MAIN);
    lv_label_set_text_fmt(lbl, "%s\n\nA definir...", label);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, lv_pct(90));
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}
