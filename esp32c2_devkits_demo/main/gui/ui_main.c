#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_partition.h"
#include "spi_flash_mmap.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "lv_demos.h"
#include "lv_examples.h"
#include "lvgl.h"

#include "esp_pm.h"
#include "iot_button.h"
#include "esp_sleep.h"
#include "esp_idf_version.h"

#include "esp_lv_spng.h"
#include "toothbrush_ui/ui.h"
#include "toothbrush_ui/ui_helpers.h"
#include "ui_main.h"

static const char *TAG = "ui_main";

/**< brushing_control, please add brush function code*/
void brushing_control(brush_status_t new_state);

/**< Display off countdown */
static uint8_t sleep_countdown_time = DISPLAY_SLEEP_TIME;
static bool sleep_status = false;
void reset_sleep_countdown_time(void)
{
    sleep_countdown_time = DISPLAY_SLEEP_TIME;
}
uint8_t get_sleep_countdown_time(void)
{
    return sleep_countdown_time;
}
void decrease_sleep_countdown_time(void)
{
    --sleep_countdown_time;
}
bool get_sleep_status(void)
{
    return sleep_status;
}

/**< Brush Timer */
TimerHandle_t g_brush_timer = NULL;
brush_second_t g_brush_remain_time = 180;
brush_second_t g_brush_use_time = 0;
brush_second_t brush_total_time = 180;

void set_brush_total_time(brush_second_t time)
{
    brush_total_time = time;
}

brush_second_t get_brush_total_time(void)
{
    return brush_total_time;
}

/**< Calculate user toothbrush time when Brushing Screen changed */
/**< Should be called before g_brush_remain_time be reset */
static void calculate_brush_time(void)
{
    g_brush_use_time = get_brush_total_time() - g_brush_remain_time;
}
/**< Get user toothbrush time should be called in Evaluation Screen */
brush_second_t ui_get_brush_time(void)
{
    return g_brush_use_time;
}

brush_time_t time_format_conversion(brush_second_t brush_time)
{
    brush_time_t format_time;
    format_time.brush_minute = brush_time / 60;
    format_time.brush_second = brush_time % 60;
    return format_time;
}

static void brush_timer_callback(TimerHandle_t xTimer)
{
    if (g_brush_remain_time <= 0) {
        brushing_control(BRUSH_STOP);
        ui_brushing_screen_clear();
    } else {
        --g_brush_remain_time;
        ui_brush_time_remain_display(g_brush_remain_time);
    }
}

/**< Evaluation_Screen automatic switching screen  */
TimerHandle_t g_auto_switch_timer = NULL;
uint8_t g_screen_wait_time = SCREEN_AUTO_SWITCH_TIME;

static void auto_switch_timer_callback(TimerHandle_t xTimer)
{
    if (g_screen_wait_time <= 0) {
        ui_evaluation_screen_clear();  
    } else {
        --g_screen_wait_time;
    }
}

void ui_printf_stack()
{
    static char s_buffer[128];
    sprintf(s_buffer, "   Biggest /     Free /    Min/    Total\n"
            "\t  SRAM : [%8d / %8d / %8d / %8d]",
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
            heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
            heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI("MEM", "%s", s_buffer);
}

/**
 * @brief Data limited
 *
 */
static int data_limited(int value, int min, int max)
{
    if (value < min) {
        return min;
    } else if (value > max) {
        return max;
    } else {
        return value;
    }
}

/**
 * @brief screen changed
 *
 */
static screen_index_t cur_screen_index = SYSTEM_START_SCREEN_INDEX;

void ui_change_to_screen(screen_index_t new_screen_index)
{
    switch (new_screen_index) {
    case SYSTEM_START_SCREEN_INDEX:
        lv_indev_wait_release(lv_indev_get_act());
        /**< NEXT SCREEN */
        _ui_screen_change(&ui_systemstart_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_systemstart_screen_init);
        cur_screen_index = SYSTEM_START_SCREEN_INDEX;
        ESP_LOGI(TAG, "SYSTEM_START_SCREEN.");
        break;
    case SYSTEM_SLEEP_SCREEN_INDEX:
        lv_indev_wait_release(lv_indev_get_act());
        _ui_screen_change(&ui_systemclose_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_systemclose_screen_init);
        cur_screen_index = SYSTEM_SLEEP_SCREEN_INDEX;
        ESP_LOGI(TAG, "SYSTEM_SLEEP_SCREEN.");
        break;
    case HOME_SCREEN_INDEX:
        lv_indev_wait_release(lv_indev_get_act());
        _ui_screen_change(&ui_home_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_home_screen_init);
        cur_screen_index = HOME_SCREEN_INDEX;
        ESP_LOGI(TAG, "HOME_SCREEN.");
        break;
    case BRUSHING_SCREEN_INDEX:
        lv_indev_wait_release(lv_indev_get_act());
        /**< Create counter Timer */
        if (g_brush_timer == NULL) {
            g_brush_timer = xTimerCreate("g_brush_timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, brush_timer_callback);
        }
        g_brush_remain_time = get_brush_total_time();
        _ui_screen_change(&ui_brushing_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_brushing_screen_init);
        cur_screen_index = BRUSHING_SCREEN_INDEX;
        ESP_LOGI(TAG, "BRUSHING_SCREEN.");
        break;
    case EVALUATION_SCREEN_INDEX:
        lv_indev_wait_release(lv_indev_get_act());
        /**< Create automatic switching screen Timer */
        if (g_auto_switch_timer == NULL) {
            g_auto_switch_timer = xTimerCreate("g_auto_switch_timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, auto_switch_timer_callback);
        }
        xTimerStart(g_auto_switch_timer, 0);
        _ui_screen_change(&ui_evaluation_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_evaluation_screen_init);
        cur_screen_index = EVALUATION_SCREEN_INDEX;
        ESP_LOGI(TAG, "EVALUATION_SCREEN.");
        break;
    case CHARGING_SCREEN_INDEX:
        lv_indev_wait_release(lv_indev_get_act());
        _ui_screen_change(&ui_charging_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_charging_screen_init);
        cur_screen_index = CHARGING_SCREEN_INDEX;
        ESP_LOGI(TAG, "CHARGING_SCREEN.");
        break;
    default:
        lv_indev_wait_release(lv_indev_get_act());
        _ui_screen_change(&ui_systemstart_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_systemstart_screen_init);
        cur_screen_index = SYSTEM_START_SCREEN_INDEX;
        ESP_LOGI(TAG, "SYSTEM_START_SCREEN.");
    }
}

screen_index_t ui_get_current_screen_index()
{
    return cur_screen_index;
}

/**< Button state */
button_state_t btn_state = {
    .is_trigger = false,
    .trigger_mode = BUTTON_NULL_PRESS
};
void ui_set_btn_state(button_trigger_mode mode)
{
    btn_state.is_trigger = true;
    btn_state.trigger_mode = mode;
}
void ui_reset_btn_state()
{
    btn_state.is_trigger = false;
    btn_state.trigger_mode = BUTTON_NULL_PRESS;
}

bool ui_get_btn_is_trigger(void)
{
    return btn_state.is_trigger;
}

button_trigger_mode ui_get_btn_trigger_mode(void)
{
    return btn_state.trigger_mode;
}

void ui_toothbrush_screen_init()
{
    ui_init();
}

/**
 * @brief wifi and bluetooth connect status
 * @param state true or false indicate wifi on/off
 */
void ui_wifi_status_display(bool state)
{
    if (state) {
        lv_img_set_src(ui_home_screen_imagewifi, &ui_img_wifion_png);
    } else {
        lv_img_set_src(ui_home_screen_imagewifi, &ui_img_wifioff_png);
    }
}

void ui_bluetooth_status_display(bool state)
{
    if (state) {
        lv_img_set_src(ui_home_screen_imagebt, &ui_img_bt_off_png);
    } else {
        lv_img_set_src(ui_home_screen_imagebt, &ui_img_bt_on_png);
    }
}

/**< Time display */
const char* weekday_str[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
/**
 * @brief Time and Date Week_day Display
 * @param hour eg. "10"
 * @param min  eg. "28"
 * @param date eg. "07/15"
 * @param day  eg. Mon
 * @example ui_time_date_display("10", "59", "08/09", Fri);    
 */
void ui_time_date_display(const char* hour, const char* min, const char* date, weekday_t day)
{
    lv_label_set_text(ui_home_screen_labeltimehour, hour);
    lv_label_set_text(ui_home_screen_labeltimesec, min);
    lv_label_set_text(ui_home_screen_labeldate, date);
    lv_label_set_text(ui_home_screen_labelweek, weekday_str[day]);
}

/**< Battery power display */

/**
 * @brief Battery power display
 * @param val 0-100(%)
 */
void ui_battery_power_display(battery_power_t val)
{
    val = data_limited(val, 0, 100);
    float v = (float)val * 0.01 * (float)BATTERY_FULL;
    if (v < BATTERY_FULL * 0.4) {
        lv_obj_set_style_bg_color(ui_home_screen_battery_value, lv_color_hex(0x8B0000), 0);
    } else if (v >= BATTERY_FULL * 0.4) {
        lv_obj_set_style_bg_color(ui_home_screen_battery_value, lv_color_hex(0x0468B), 0);
    }
    lv_obj_set_width(ui_home_screen_battery_value, (int)v); // Set battery value
    sprintf(battery_value_str, "%d%%", (int)(val));
    lv_label_set_text(ui_home_screen_labelpwr, battery_value_str);
}

/**< Countdown to brush-tooth */
/**
 * @brief brush-tooth remain time display
 * @param val remain time(s)
 */
void ui_brush_time_remain_display(brush_second_t val)
{
    val = data_limited(val, 0, get_brush_total_time());
    brush_time_t brush_time =  time_format_conversion(val);
    sprintf(brushtooth_remain_time_str, "%02d : %02d", (int)brush_time.brush_minute, (int)brush_time.brush_second);
    lv_label_set_text(ui_brushing_screen_labelremaintime, brushtooth_remain_time_str);

    double arc_remain_rate = (double)val / (double)get_brush_total_time();
    lv_arc_set_value(ui_brushing_screen_arcbrushtime, (int)(arc_remain_rate * 360));
}

/**
 * @brief brush-tooth status change
 * @param state BRUSH_START BRUSH_STOP
 */
void ui_brush_status_change(brush_status_t state)
{
    if (BRUSH_START == state) {
        lv_img_set_src(ui_brushing_screen_imagestart, &ui_img_start_brush_icon_png);
        lv_obj_set_style_arc_color(ui_brushing_screen_arcbrushtime, lv_color_hex(0x87CEEB), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_label_set_text(ui_brushing_screen_label_prompt1, "Double-Level");
        lv_label_set_text(ui_brushing_screen_label_prompt2, "Single-Pause");
    } else {
        lv_img_set_src(ui_brushing_screen_imagestart, &ui_img_stop_brush_icon_png);
        lv_obj_set_style_arc_color(ui_brushing_screen_arcbrushtime, lv_color_hex(0xDC143C), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_label_set_text(ui_brushing_screen_label_prompt1, "LongPre-Exit");
        lv_label_set_text(ui_brushing_screen_label_prompt2, "Single-Start");
    }
}

/**< Toothbrush Strength set */
extern lv_obj_t* g_brush_strength_value[BRUSH_STRENTH_MAX];
/**< Record the intensity of brushing teeth */ 
static brush_strength_t strength_flag = STRENGTH_LEVEL_1; 

/**
 * @brief Brushtooth strength initialization: STRENGTH_LEVEL_1
 */
void brush_strength_init(void)
{
    // strength_flag = STRENGTH_LEVEL_1;
    brush_strength_set(strength_flag);
}

/**< Brushtooth strength set */
void brush_strength_set(brush_strength_t strength)
{
    /**< Displayed Objects */
    for (int i = 0; i < strength; ++i) {
        lv_obj_set_style_bg_opa(g_brush_strength_value[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    /**< Not displayed Objects */
    for (int i = strength; i < BRUSH_STRENTH_MAX; ++i) {
        lv_obj_set_style_bg_opa(g_brush_strength_value[i], 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    strength_flag = strength;
}

/**< Brushtooth strength get */
brush_strength_t brush_strength_get(void)
{
    return strength_flag;
}

/**< Brushtooth strength shifted from 1 --- 4 */
void ui_brush_strength_shift(void)
{
    if (strength_flag == STRENGTH_LEVEL_4) {
        strength_flag = STRENGTH_LEVEL_1;
    } else {
        ++strength_flag;
    }
    brush_strength_set(strength_flag);
}

/**< Clear Brush_Screen and Change to Evaluation_Screen */
void ui_brushing_screen_clear(void)
{
    calculate_brush_time();
    if (xTimerIsTimerActive(g_brush_timer) != pdFALSE) {
        /* xTimer is active, do something. */
        xTimerStop(g_brush_timer, 0);
    }
    if (g_brush_timer != NULL)
        xTimerDelete(g_brush_timer, portMAX_DELAY);
    g_brush_timer = NULL;
    g_brush_remain_time = get_brush_total_time();

    ui_change_to_screen(EVALUATION_SCREEN_INDEX);
    lv_obj_del(ui_brushing_screen);
    ui_brushing_screen = NULL;
}

/**< Clear Evaluation Screen and Change to Home_Screen */
/**< Not delete the screen */
void ui_evaluation_screen_clear(void) 
{
    if (xTimerIsTimerActive(g_auto_switch_timer) != pdFALSE) {
        /* xTimer is active, do something. */
        xTimerStop(g_auto_switch_timer, 0);
    }
    if (g_auto_switch_timer != NULL)
        xTimerDelete(g_auto_switch_timer, portMAX_DELAY);
    g_auto_switch_timer = NULL;
    g_screen_wait_time = SCREEN_AUTO_SWITCH_TIME;

    ui_change_to_screen(HOME_SCREEN_INDEX);
}

/**< Brush evaluation */
static const char* evaluation_str[] = {
    "Perfect!", "Good!", "Common", "Fair~", "Poor~"
};
const lv_img_dsc_t* ui_imgset_effect[5] = {&ui_img_effect0_png, &ui_img_effect1_png, &ui_img_effect2_png, &ui_img_effect3_png, &ui_img_effect4_png};

/**
 * @brief Brushtooth effect evaluate
 * @param stage eg. FIRST_STAGE...
 */
void ui_brush_evaluation(brush_effect_t stage)
{
    lv_label_set_text(ui_evaluation_screen_labelevalue, evaluation_str[stage]);
    lv_img_set_src(ui_evaluation_screen_imagetooth, ui_imgset_effect[stage]);
}

/**
 * @brief Charging power display
 * @param val 0-100(%), charging power
 */
void ui_charging_power_display(battery_power_t val)
{
    val = data_limited(val, 0, 100);
    sprintf(charging_value_str, "%d%%", (int)val);
    lv_label_set_text(ui_charging_screen_labelcharging, charging_value_str);
}

extern lv_disp_t * g_dispp;
/**
 * @brief UI closed in SYSTEM_SLEEP_SCREEN
 */
void ui_closed(void)
{
    lv_obj_add_flag(ui_systemclose_screen, LV_OBJ_FLAG_HIDDEN);
    lv_disp_set_bg_color(g_dispp, lv_palette_main(LV_PALETTE_NONE));
}

/**
 * @brief UI Restart when BUTTON_LONG_PRESS
 */
void ui_restart(void)
{
    ui_change_to_screen(SYSTEM_START_SCREEN_INDEX);
    lv_obj_clear_flag(ui_systemclose_screen, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief UI display off
 * @param screen_index
 */
void ui_display_off(screen_index_t screen_index)
{
    lv_obj_add_flag(ui_screen[screen_index], LV_OBJ_FLAG_HIDDEN);
    lv_disp_set_bg_color(g_dispp, lv_palette_main(LV_PALETTE_NONE));
    sleep_status = true;
}

/**
 * @brief UI display on
 * @param screen_index
 */
void ui_display_on(screen_index_t screen_index)
{
    lv_obj_clear_flag(ui_screen[screen_index], LV_OBJ_FLAG_HIDDEN);
    sleep_status = false;
}
