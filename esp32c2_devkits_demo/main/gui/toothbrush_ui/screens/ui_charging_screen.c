// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.1
// LVGL version: 8.3.11
// Project name: Eletric_toothbrush

#include "../ui.h"

static void charging_screen_arc_anim_cb(void* obj, int32_t v) 
{
    lv_obj_set_style_arc_opa(obj, v, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

static void image_charging_screen_anim_cb(void * var, int32_t v) {
    lv_obj_t * obj = (lv_obj_t *)var; 
    lv_img_set_zoom(obj, (uint16_t)v); 
}

char charging_value_str[10]; /**< Charging power label str */
void ui_charging_screen_init(void)
{
    ui_charging_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_charging_screen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_charging_screen_arccharging = lv_arc_create(ui_charging_screen);
    lv_obj_set_width(ui_charging_screen_arccharging, 60);
    lv_obj_set_height(ui_charging_screen_arccharging, 60);
    lv_obj_set_x(ui_charging_screen_arccharging, 0);
    lv_obj_set_y(ui_charging_screen_arccharging, -10);
    lv_obj_set_align(ui_charging_screen_arccharging, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_charging_screen_arccharging, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_charging_screen_arccharging, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_arc_set_range(ui_charging_screen_arccharging, 0, 360);
    lv_arc_set_value(ui_charging_screen_arccharging, 360);
    lv_arc_set_rotation(ui_charging_screen_arccharging, 270);
    lv_arc_set_bg_angles(ui_charging_screen_arccharging, 0, 360);
    lv_obj_remove_style(ui_charging_screen_arccharging, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ui_charging_screen_arccharging, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_set_style_arc_width(ui_charging_screen_arccharging, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_color(ui_charging_screen_arccharging, lv_color_hex(0x00FFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_charging_screen_arccharging, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_charging_screen_arccharging, 10, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_anim_t charging_screen_arc_anim;
    lv_anim_init(&charging_screen_arc_anim);
    lv_anim_set_var(&charging_screen_arc_anim, ui_charging_screen_arccharging);
    lv_anim_set_exec_cb(&charging_screen_arc_anim, charging_screen_arc_anim_cb);
    lv_anim_set_time(&charging_screen_arc_anim, 1000);
    lv_anim_set_repeat_count(&charging_screen_arc_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&charging_screen_arc_anim, 0);
    lv_anim_set_playback_delay(&charging_screen_arc_anim, 0);
    lv_anim_set_playback_time(&charging_screen_arc_anim, 1000);
    lv_anim_set_values(&charging_screen_arc_anim, 80, 255);
    lv_anim_start(&charging_screen_arc_anim);

    ui_charging_screen_imagecharging = lv_img_create(ui_charging_screen_arccharging);
    lv_img_set_src(ui_charging_screen_imagecharging, &ui_img_power_icon_png);
    lv_obj_set_width(ui_charging_screen_imagecharging, LV_SIZE_CONTENT);   /// 50
    lv_obj_set_height(ui_charging_screen_imagecharging, LV_SIZE_CONTENT);    /// 50
    lv_obj_set_align(ui_charging_screen_imagecharging, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_charging_screen_imagecharging, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_charging_screen_imagecharging, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_anim_t image_charging_screen_anim;
    lv_anim_init(&image_charging_screen_anim);
    lv_anim_set_var(&image_charging_screen_anim, ui_charging_screen_imagecharging);
    lv_anim_set_exec_cb(&image_charging_screen_anim, image_charging_screen_anim_cb);
    lv_anim_set_time(&image_charging_screen_anim, 1000);
    lv_anim_set_values(&image_charging_screen_anim, 200, 240);
    lv_anim_set_repeat_count(&image_charging_screen_anim, LV_ANIM_REPEAT_INFINITE);    /*Just for the demo*/
    lv_anim_set_repeat_delay(&image_charging_screen_anim, 0);
    lv_anim_set_playback_time(&image_charging_screen_anim, 1000);
    lv_anim_set_path_cb(&image_charging_screen_anim, lv_anim_path_ease_in_out);
    lv_anim_start(&image_charging_screen_anim);

    ui_charging_screen_labelcharging = lv_label_create(ui_charging_screen);
    lv_obj_set_width(ui_charging_screen_labelcharging, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_charging_screen_labelcharging, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_charging_screen_labelcharging, 0);
    lv_obj_set_y(ui_charging_screen_labelcharging, 40);
    lv_obj_set_align(ui_charging_screen_labelcharging, LV_ALIGN_CENTER);
    lv_label_set_text(ui_charging_screen_labelcharging, "75%");
    lv_obj_set_style_text_font(ui_charging_screen_labelcharging, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
}
