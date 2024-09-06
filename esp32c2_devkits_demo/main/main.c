/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

static const char *TAG = "main";

static button_handle_t *g_btn_handle = NULL;

static void btn_select_sw_cb(void *handle, void *arg)
{
    ESP_LOGI("BTN", "select switch");
    // vTaskDelay(pdMS_TO_TICKS(2*1000));
    esp_restart();
}

void printf_stack()
{
    static char buffer[128];
    sprintf(buffer, "   Biggest /     Free /    Min/    Total\n"
            "\t  SRAM : [%8d / %8d / %8d / %8d]",
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
            heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
            heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI("MEM", "%s", buffer);
}

esp_err_t app_btn_init(void)
{
    ESP_ERROR_CHECK((NULL != g_btn_handle));

    int btn_num = 0;
    g_btn_handle = calloc(sizeof(button_handle_t), (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM));
    assert((g_btn_handle) && "memory is insufficient for button");
    return bsp_iot_button_create(g_btn_handle, &btn_num, (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM));
}

esp_err_t app_btn_register_callback(bsp_button_t btn, button_event_t event, button_cb_t callback, void *user_data)
{
    assert((g_btn_handle) && "button not initialized");
    assert((btn < (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM)) && "button id incorrect");

    if (NULL == callback) {
        return iot_button_unregister_cb(g_btn_handle[btn], event);
    }
    return iot_button_register_cb(g_btn_handle[btn], event, callback, user_data);
}

esp_err_t app_btn_register_event_callback(bsp_button_t btn, button_event_config_t cfg, button_cb_t callback, void *user_data)
{
    assert((g_btn_handle) && "button not initialized");
    esp_err_t err = iot_button_register_event_cb(g_btn_handle[btn], cfg, callback, (void *)(cfg.event));
    ESP_ERROR_CHECK(err);
    return ESP_OK;
}

esp_err_t app_btn_rm_all_callback(bsp_button_t btn)
{
    assert((g_btn_handle) && "button not initialized");
    assert((btn < (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM)) && "button id incorrect");

    for (size_t event = 0; event < BUTTON_EVENT_MAX; event++) {
        iot_button_unregister_cb(g_btn_handle[btn], event);
    }
    return ESP_OK;
}

esp_err_t app_btn_rm_event_callback(bsp_button_t btn, size_t event)
{
    assert((g_btn_handle) && "button not initialized");
    assert((btn < (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM)) && "button id incorrect");

    iot_button_unregister_cb(g_btn_handle[btn], event);
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    printf_stack();
    bsp_display_start();

    app_btn_init();
    app_btn_register_callback(BSP_BUTTON_CONFIG, BUTTON_PRESS_UP, btn_select_sw_cb, NULL);
    app_btn_register_callback(BSP_BUTTON_CONFIG, BUTTON_PRESS_DOWN, btn_select_sw_cb, NULL);

    if (button_gpio_get_key_level((void *)BSP_LCD_SELECT)) {
        ESP_LOGI(TAG, "enter 1.28 screen");
        ui_1_28_start();
    } else {
        ESP_LOGI(TAG, "enter 0.96 screen");
        toothbrush_start();
        return;
    }
}
