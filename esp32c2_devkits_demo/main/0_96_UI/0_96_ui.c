/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "lv_demos.h"
#include "lv_examples.h"
#include "lvgl.h"

#include "esp_log.h"
#include "esp_partition.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_idf_version.h"
#include "esp_lv_spng.h"
#include "nvs_flash.h"

#include "spi_flash_mmap.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "iot_button.h"
#include "driver/gpio.h"

#include "buzzer.h"
#include "motor.h"
#include "power.h"
// #include "toothbrush_ui/ui.h"
// #include "toothbrush_ui/ui_helpers.h"
#include "gui/ui_main.h"

#include "rainmaker.h"

/**< System Time */
time_t now;
struct tm timeinfo;
/**< System Time Initialization */
void time_init(void) 
{    
    /** Set local time */
    struct tm t = {0};
    t.tm_year = 2024 - 1900;
    t.tm_mon = 8 - 1;
    t.tm_mday = 9;
    time_t timeSinceEpoch = mktime(&t);
    struct timeval local_now = { .tv_sec = timeSinceEpoch };
    settimeofday(&local_now, NULL);
}

void time_refresh(struct tm timeinfo)
{
    time(&now);
    // 将时区设置为中国标准时间
    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    char hour[5];
    sprintf(hour, "%02d", timeinfo.tm_hour);
    char min[5];
    sprintf(min, "%02d", timeinfo.tm_min);
    char date[15];
    sprintf(date, "%02d/%02d", timeinfo.tm_mon + 1, timeinfo.tm_mday);
    ui_time_date_display(hour, min, date, (weekday_t)timeinfo.tm_wday);
}

#define POWER_CHARGE_STATUS_PIN         GPIO_NUM_5       // Charge status pin

static char power_str[] = "%d %";
static const char *TAG = "0_96_ui";
static QueueHandle_t brush_charge_queue = NULL;     // brush charge queue
static brush_status_t brush_state = BRUSH_STOP;     // brushing state
static bool is_charge_status = false;               // charge status
static bool is_rainmaker_connected = false;         // rainmaker connected status
motor_frequency_level_t brush_frequency_level = MOTOR_FREQUENCY_LEVEL_3;
motor_amplitude_level_t brush_amplitude_level = MOTOR_AMPLITUDE_LEVEL_1;

static esp_rmaker_device_t *g_rainmaker;
/**< brushing_control, please add brush function code*/
void brushing_control(brush_status_t new_state);
/**< UI start, called when the button is triggered, function code filling position */
void ui_start(void);

/**
 * @brief ToothBrush button registered
 *
 */
#define BUTTON_ACTIVE_LEVEL     0
typedef enum {
    BRUSH_BUTTON_NONE = -1,
    BRUSH_BUTTON_SINGLE_CLICK = 4,
    BRUSH_BUTTON_DOUBLE_CLICK = 5,    
    BRUSH_BUTTON_MULTIPLE_CLICK = 6,
    BRUSH_BUTTON_LONG_PRESS_START = 7,
} brush_button_event_t;


void button_event_cb(void *arg, void *data)
{
    if (get_sleep_status() == false) {
        if ((brush_button_event_t)data == BRUSH_BUTTON_SINGLE_CLICK) {
            /**< Add Beep control code */
            buzzer_play_audio(AUDIO_INDEX_SINGLE_PRESS);
            ESP_LOGD(TAG, "BUTTON_SINGLE_CLICK");
            ui_set_btn_state(BUTTON_SHORT_PRESS);
        } else if ((brush_button_event_t)data == BRUSH_BUTTON_LONG_PRESS_START) {
            /**< Add Beep control code */
            buzzer_play_audio(AUDIO_INDEX_LONG_PRESS);
            ESP_LOGD(TAG, "BUTTON_LONG_PRESS");
            ui_set_btn_state(BUTTON_LONG_PRESS);
        } else if ((brush_button_event_t)data == BRUSH_BUTTON_DOUBLE_CLICK) {
            /**< Add Beep control code */
            buzzer_play_audio(AUDIO_INDEX_DOUBLE_PRESS);
            ESP_LOGD(TAG, "BRUSH_BUTTON_DOUBLE_CLICK");
            /**< BRUSHING_SCREEN double click brush strength shift */
            if (BRUSHING_SCREEN_INDEX == ui_get_current_screen_index()) {
                /**< Add strength shift code */
                ui_brush_strength_shift();
                brush_frequency_level = brush_strength_get();
                motor_set_params(brush_frequency_level, brush_amplitude_level);
            }
        } else if ((brush_button_event_t)data == BRUSH_BUTTON_MULTIPLE_CLICK) {
            ESP_LOGD(TAG, "BRUSH_BUTTON_MULTIPLE_CLICK");
            nvs_flash_erase();
            esp_restart();
        }

        if (BUTTON_SHORT_PRESS == ui_get_btn_trigger_mode() || BUTTON_LONG_PRESS == ui_get_btn_trigger_mode()) {
            ui_start();
            ui_reset_btn_state();
        }
    
        /**< Refresh rainmaker */
        if (g_rainmaker != NULL && is_rainmaker_connected) {
            ESP_LOGI(TAG,"Brushing Mode %s ",rainmaker_get_brushing_mode_str(brush_strength_get()-1));
            esp_rmaker_param_update_and_report(
                        esp_rmaker_device_get_param_by_name(g_rainmaker, "Brushing Mode"),
                        esp_rmaker_str(rainmaker_get_brushing_mode_str(brush_strength_get()-1))); 

            sprintf(power_str, "%d %%", get_power_value());
            esp_rmaker_param_update_and_report(
                        esp_rmaker_device_get_param_by_name(g_rainmaker, "Battery Level"),
                        esp_rmaker_str(power_str)); 
            
            esp_rmaker_param_update_and_report(
                        esp_rmaker_device_get_param_by_name(g_rainmaker, "Brushing time(s)"),
                        esp_rmaker_float((float)ui_get_brush_time())); 
        }

        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
            ESP_LOGI(TAG, "Wake up from light sleep, reason %d", cause);
        }
    }
    /**< Reset display countdown time */
    reset_sleep_countdown_time();
    if (get_sleep_status() == true) {
        ui_display_on(ui_get_current_screen_index());
    }
}

/**
 * @brief brushing_control, please add brush function code
 * @param new_state new brushing state
 */
void brushing_control(brush_status_t new_state)
{
    brush_state = motor_get_state();
    if (new_state == brush_state) {
        return;
    }
    brush_state = new_state;
    if (brush_state == BRUSH_START) {
        if (xTimerIsTimerActive(g_brush_timer) != pdFALSE) {
            /* Timer abnormal */
            ESP_LOGE(TAG, "g_brush_timer has been start!");
            return;
        } else {
            xTimerStart(g_brush_timer, 0);
            ui_brush_status_change(BRUSH_START);
            /**< Add start brush function >**/
            vTaskDelay(pdMS_TO_TICKS(50));
            brush_frequency_level = brush_strength_get();
            motor_set_params(brush_frequency_level, brush_amplitude_level);
            motor_start();
            buzzer_play_audio(AUDIO_INDEX_START);
            /**< Add start brush function >**/
        }
    } else {
        if (xTimerIsTimerActive(g_brush_timer) != pdFALSE) {
            xTimerStop(g_brush_timer, 0);
            ui_brush_status_change(BRUSH_STOP);
            /**< Add stop brush function >**/
            vTaskDelay(pdMS_TO_TICKS(50));
            motor_stop();
            buzzer_play_audio(AUDIO_INDEX_END);
            /**< Add stop brush function >**/
        }
    }
}

static esp_err_t rainmaker_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
        const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) { /* If this is not a null pointer, print the data source */
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    const char *device_name = esp_rmaker_device_get_name(device);
    const char *param_name = esp_rmaker_param_get_name(param);

    ESP_LOGD(TAG, "Device name: %s, Device param: %s, Val: %d", device_name, param_name,val.val.i);

    if (strcmp(param_name, "Brushing Mode") == 0) {
        ESP_LOGI(TAG,"Brushing Mode %s ",val.val.s);
        ESP_LOGD(TAG,"Brushing Mode %d ",rainmaker_get_brushing_mode_num(val.val.s));
        brush_strength_set(rainmaker_get_brushing_mode_num(val.val.s));
        brush_frequency_level = brush_strength_get();
        motor_set_params(brush_frequency_level, brush_amplitude_level);
    } else if (strcmp(param_name, "Set brush time(s)") == 0) {
        ESP_LOGI(TAG,"Set brush time(s) %d",val.val.i);
        set_brush_total_time(val.val.i);
        esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_name(g_rainmaker, "Set brush time(s)"),
                esp_rmaker_int(val.val.i)); 
    } 

    return ESP_OK;
}



/**
 * @brief UI start, called when the button is triggered, program for realizing each function
 * @param void
 */
void ui_start()
{
    /**< Wait system_on animation finished and change to home_screen */
    /**< Single thread need not to wait */
    /**< Get current screen_index */
    screen_index_t cur_src_index = ui_get_current_screen_index();
    switch (cur_src_index) {
    /**< SYSTEM_START SCREEN */
    case SYSTEM_START_SCREEN_INDEX:
        if (BUTTON_SHORT_PRESS == ui_get_btn_trigger_mode()) {
            /**< Short pressed */
        } else if (BUTTON_LONG_PRESS == ui_get_btn_trigger_mode()) {
            /**< Long pressed */
        }
        break;
    /**< HOME SCREEN */
    case HOME_SCREEN_INDEX:
        if (BUTTON_SHORT_PRESS == ui_get_btn_trigger_mode()) {
            /**< Short pressed */
            ui_change_to_screen(BRUSHING_SCREEN_INDEX);
            brushing_control(BRUSH_START);
        } else if (BUTTON_LONG_PRESS == ui_get_btn_trigger_mode()) {
            /**< Long pressed */
            ui_change_to_screen(SYSTEM_SLEEP_SCREEN_INDEX);
        }
        break;
    /**< BRUSHING SCREEN */
    case BRUSHING_SCREEN_INDEX:
        if (BUTTON_SHORT_PRESS == ui_get_btn_trigger_mode()) {
            /**< Short pressed */
            /**< Toothbrushing state switching */
            /**< Add toothbrush code in brushing_control() >**/
            if (BRUSH_STOP == brush_state) {
                brushing_control(BRUSH_START);
            } else {
                brushing_control(BRUSH_STOP);
            }
        } else if (BUTTON_LONG_PRESS == ui_get_btn_trigger_mode()) {
            /**< Long pressed */
            brushing_control(BRUSH_STOP);
            ui_brushing_screen_clear();
            ESP_LOGI(TAG, "This brush_time: %ds", (int)ui_get_brush_time());
        }
        break;
    /**< EVALUATION SCREEN */
    case EVALUATION_SCREEN_INDEX:
        if (BUTTON_SHORT_PRESS == ui_get_btn_trigger_mode()) {
            /**< Short pressed */
            ui_evaluation_screen_clear();
        } else if (BUTTON_LONG_PRESS == ui_get_btn_trigger_mode()) {
            /**< Long pressed */
            ui_change_to_screen(SYSTEM_SLEEP_SCREEN_INDEX);
        }
        break;
    /**< CHARGING SCREEN */
    case CHARGING_SCREEN_INDEX:
        if (BUTTON_SHORT_PRESS == ui_get_btn_trigger_mode()) {
            /**< Short pressed */
            ui_change_to_screen(HOME_SCREEN_INDEX);
        } else if (BUTTON_LONG_PRESS == ui_get_btn_trigger_mode()) {
            /**< Long pressed */
            ui_change_to_screen(SYSTEM_SLEEP_SCREEN_INDEX);
        }
        break;
    /**< SYSTEM OFF SCREEN */
    case SYSTEM_SLEEP_SCREEN_INDEX:
        if (BUTTON_SHORT_PRESS == ui_get_btn_trigger_mode()) {
            /**< Short pressed */
            /**< System restart */
            ui_closed();

        } else if (BUTTON_LONG_PRESS == ui_get_btn_trigger_mode()) {
            /**< Long pressed */
            /**< System off */
            ESP_ERROR_CHECK(nvs_flash_erase());
            nvs_flash_init();
            ui_restart();
        }
        break;
    }
}

/**
 * @brief Charge status GPIO ISR handler
 * @param arg GPIO number
 */
void IRAM_ATTR charge_status_gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(brush_charge_queue, &gpio_num, NULL);
}

/**
 * @brief Charge status GPIO initialization
 * @param void
 */
void charge_status_gpio_init(void)
{
    //-------------GPIO Init---------------//    
    gpio_config_t io_conf = {
       .intr_type = GPIO_INTR_ANYEDGE,
       .mode = GPIO_MODE_INPUT,
       .pin_bit_mask = 1ULL << POWER_CHARGE_STATUS_PIN,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(POWER_CHARGE_STATUS_PIN, charge_status_gpio_isr_handler, (void*) POWER_CHARGE_STATUS_PIN));
}

/**
 * @brief Charge status task
 * @param arg void
 */
void ui_display_change_task(void *arg)
{
    uint32_t gpio_num = 0;
    uint32_t time_s = 0;
    while (1) {
        time_s+=2;
        int power = get_power_value();
        ESP_LOGD(TAG, "ADC power: %d ", power);
        
        bsp_display_lock(0);
        if (xQueueReceive(brush_charge_queue, &gpio_num, 0) == pdTRUE) {
            int gpio_level = gpio_get_level(POWER_CHARGE_STATUS_PIN);
            if (gpio_level == 1) {  
                ui_charging_power_display(power);
                ui_change_to_screen(HOME_SCREEN_INDEX);
                is_charge_status = false;
            } else {  
                ui_change_to_screen(CHARGING_SCREEN_INDEX);
                is_charge_status = true;
            }
            esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_name(g_rainmaker, "state of charge"),
                    esp_rmaker_bool(is_charge_status)); 
        }
        ui_charging_power_display(power);
        ui_battery_power_display(power);
        if (power <= 20)
        {
            esp_rmaker_raise_alert("Please connect the charger");
        }
        
        is_rainmaker_connected = rainmaker_get_connect_status();
        ui_wifi_status_display(is_rainmaker_connected);
        ui_bluetooth_status_display(is_rainmaker_connected);
        time_refresh(timeinfo);
        bsp_display_unlock();
        // ESP_LOGI(TAG, "UI Running!");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void toothbrush_start(void)
{
    ui_printf_stack();
    time_init();
    bsp_display_lock(0);
    ui_toothbrush_screen_init();
    button_event_config_t event_cfg = {
        .event = BUTTON_MULTIPLE_CLICK,
        .event_data.multiple_clicks.clicks = 3,
    };
    app_btn_register_event_callback(BSP_BUTTON_NUM + BSP_ADC_BUTTON_ENTER, event_cfg, button_event_cb, NULL);
    app_btn_register_callback(BSP_BUTTON_NUM + BSP_ADC_BUTTON_ENTER, BUTTON_SINGLE_CLICK, button_event_cb, (void *)BUTTON_SINGLE_CLICK);
    app_btn_register_callback(BSP_BUTTON_NUM + BSP_ADC_BUTTON_ENTER, BUTTON_DOUBLE_CLICK, button_event_cb, (void *)BUTTON_DOUBLE_CLICK);
    app_btn_register_callback(BSP_BUTTON_NUM + BSP_ADC_BUTTON_ENTER, BUTTON_LONG_PRESS_START, button_event_cb, (void *)BUTTON_LONG_PRESS_START);

    bsp_display_unlock();
    buzzer_init(BUZZER_PIN_NUM);
    motor_driver_init(MOTOR_DRIVER_INPUT_A, MOTOR_DRIVER_INPUT_B);
    power_init();
    brush_charge_queue = xQueueCreate(10, sizeof(uint32_t));
    charge_status_gpio_init();

    is_rainmaker_connected = rainmaker_get_connect_status();
    ui_wifi_status_display(is_rainmaker_connected);
    ui_bluetooth_status_display(is_rainmaker_connected);
    g_rainmaker = rainmaker_init(rainmaker_write_cb);
    if (g_rainmaker == NULL)
    {
        ESP_LOGW(TAG,"rainmaker_init failed");
    }
    
    xTaskCreate(ui_display_change_task, "ui_display_change_task", 2048, NULL, 10, NULL);
    rainmaker_wait_connect();
    reset_sleep_countdown_time();

    uint8_t count = 0;
    while (1) {

        if (get_sleep_countdown_time() == 0 && ui_get_current_screen_index() != BRUSHING_SCREEN_INDEX) {
            bsp_display_lock(0);
            ui_display_off(ui_get_current_screen_index());
            bsp_display_unlock();
            reset_sleep_countdown_time();
        }
        else {
            decrease_sleep_countdown_time();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        if((count++)%10 == 0){
            printf_stack();
        }
    }

}
