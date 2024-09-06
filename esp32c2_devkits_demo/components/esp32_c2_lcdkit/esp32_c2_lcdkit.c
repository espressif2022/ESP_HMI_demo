/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/rmt_tx.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_err_check.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lvgl_port.h"

static const char *TAG = "ESP32-C3-LCDKit";

static lv_indev_t *disp_indev = NULL;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static adc_oneshot_unit_handle_t bsp_adc_handle = NULL;
#endif

static const button_config_t bsp_button_config[BSP_BUTTON_NUM] = {
    {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config.active_level = false,
        .gpio_button_config.gpio_num = BSP_LCD_SELECT,
    },
};

static const button_config_t bsp_adc_button_config[BSP_ADC_BUTTON_NUM] = {
    {
        .type = BUTTON_TYPE_ADC,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .adc_button_config.adc_handle = &bsp_adc_handle,
#endif
        .adc_button_config.adc_channel = ADC_CHANNEL_4, // ADC1 channel 4 is GPIO4
        .adc_button_config.button_index = BSP_ADC_BUTTON_PREV,
        .adc_button_config.min = 2300,
        .adc_button_config.max = 2550
    },
    {
        .type = BUTTON_TYPE_ADC,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .adc_button_config.adc_handle = &bsp_adc_handle,
#endif
        .adc_button_config.adc_channel = ADC_CHANNEL_4, // ADC1 channel 4 is GPIO4
        .adc_button_config.button_index = BSP_ADC_BUTTON_ENTER,
        .adc_button_config.min = 1050,
        .adc_button_config.max = 1300
    },
    {
        .type = BUTTON_TYPE_ADC,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .adc_button_config.adc_handle = &bsp_adc_handle,
#endif
        .adc_button_config.adc_channel = ADC_CHANNEL_4, // ADC1 channel 4 is GPIO4
        .adc_button_config.button_index = BSP_ADC_BUTTON_NEXT,
        .adc_button_config.min = 250,
        .adc_button_config.max = 500
    },
};

static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, NULL, 0, 120},     // Sleep out, Delay 120ms
    {0xB1, (uint8_t []){0x05, 0x3A, 0x3A}, 3, 0},
    {0xB2, (uint8_t []){0x05, 0x3A, 0x3A}, 3, 0},
    {0xB3, (uint8_t []){0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A}, 6, 0},
    {0xB4, (uint8_t []){0x03}, 1, 0},   // Dot inversion
    {0xC0, (uint8_t []){0x44, 0x04, 0x04}, 3, 0},
    {0xC1, (uint8_t []){0xC0}, 1, 0},
    {0xC2, (uint8_t []){0x0D, 0x00}, 2, 0},
    {0xC3, (uint8_t []){0x8D, 0x6A}, 2, 0},
    {0xC4, (uint8_t []){0x8D, 0xEE}, 2, 0},
    {0xC5, (uint8_t []){0x08}, 1, 0},
    {0xE0, (uint8_t []){0x0F, 0x10, 0x03, 0x03, 0x07, 0x02, 0x00, 0x02, 0x07, 0x0C, 0x13, 0x38, 0x0A, 0x0E, 0x03, 0x10}, 16, 0},
    {0xE1, (uint8_t []){0x10, 0x0B, 0x04, 0x04, 0x10, 0x03, 0x00, 0x03, 0x03, 0x09, 0x17, 0x33, 0x0B, 0x0C, 0x06, 0x10}, 16, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0x3A, (uint8_t []){0x05}, 1, 0},
    {0x36, (uint8_t []){0xC8}, 1, 0},
    {0x29, NULL, 0, 0},     // Display on
    {0x2C, NULL, 0, 0},     // Memory write
};

esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

static lv_disp_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;
    bsp_display_config_t bsp_disp_cfg;

    if (button_gpio_get_key_level((void *)BSP_LCD_SELECT)) {
        bsp_disp_cfg.max_transfer_sz = BSP_LCD_0_9_6_H_RES * 80 * sizeof(uint16_t);
    } else {
        bsp_disp_cfg.max_transfer_sz = BSP_LCD_1_2_8_H_RES * 80 * sizeof(uint16_t);
    }
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle));

    esp_lcd_panel_disp_on_off(panel_handle, true);

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = cfg->buffer_size,
        .double_buffer = cfg->double_buffer,
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = cfg->flags.buff_dma,
            .buff_spiram = cfg->flags.buff_spiram,
        }
    };

    if (button_gpio_get_key_level((void *)BSP_LCD_SELECT)) {
        disp_cfg.hres = BSP_LCD_1_2_8_H_RES;
        disp_cfg.vres = BSP_LCD_1_2_8_V_RES;
    } else {
        disp_cfg.hres = BSP_LCD_0_9_6_H_RES;
        disp_cfg.vres = BSP_LCD_0_9_6_V_RES;
    }

    return lvgl_port_add_disp(&disp_cfg);
}

// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}


void bsp_set_cs_low()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<BSP_LCD_CS);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    gpio_set_level(BSP_LCD_CS, 0);

}
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    assert(config != NULL && config->max_transfer_sz > 0);

    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_PCLK,
        .mosi_io_num = BSP_LCD_DATA0,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = config->max_transfer_sz,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_CS,
        // .cs_gpio_num = GPIO_NUM_NC,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, ret_io), err, TAG, "New panel IO failed");

    // bsp_set_cs_low();

    ESP_LOGD(TAG, "Install LCD driver");
    const ili9341_vendor_config_t vendor_config = {
        .init_cmds = &vendor_specific_init[0],
        .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST, // Shared with Touch reset
        .flags.reset_active_high = false,
        .color_space = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };

    ESP_LOGW(TAG, "New panel, select:%s", button_gpio_get_key_level((void *)BSP_LCD_SELECT) ? "right" : "left");

    if (button_gpio_get_key_level((void *)BSP_LCD_SELECT)) {
        panel_config.vendor_config = NULL;
        ESP_GOTO_ON_ERROR(esp_lcd_new_panel_gc9a01(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");
    } else {
        panel_config.vendor_config = (void *) &vendor_config;
        ESP_GOTO_ON_ERROR(esp_lcd_new_panel_ili9341(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");
    }

    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_reset(*ret_panel));
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_init(*ret_panel));
    if (button_gpio_get_key_level((void *)BSP_LCD_SELECT)) {
        BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_invert_color(*ret_panel, true));
        BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_mirror(*ret_panel, true, false));
    } else {
        BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_set_gap(*ret_panel, 24, 0));
        BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_mirror(*ret_panel, true, true));
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_disp_on_off(*ret_panel, true));
#else
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_disp_off(*ret_panel, false));
#endif

    return ret;

err:
    if (*ret_panel) {
        esp_lcd_panel_del(*ret_panel);
    }
    if (*ret_io) {
        esp_lcd_panel_io_del(*ret_io);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
    return ret;
}

#if 0
static lv_indev_t *bsp_display_indev_init(lv_disp_t *disp)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    /* Initialize ADC and get ADC handle */
    BSP_ERROR_CHECK_RETURN_NULL(bsp_adc_initialize());
    bsp_adc_handle = bsp_adc_get_handle();
#endif

    const lvgl_port_nav_btns_cfg_t btns = {
        .disp = disp,
        .button_prev = &bsp_adc_button_config[BSP_ADC_BUTTON_PREV],
        .button_next = &bsp_adc_button_config[BSP_ADC_BUTTON_NEXT],
        .button_enter = &bsp_adc_button_config[BSP_ADC_BUTTON_ENTER]
    };

    return lvgl_port_add_navigation_buttons(&btns);
}
#endif
lv_disp_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
#if CONFIG_BSP_LCD_DRAW_BUF_DOUBLE
        .double_buffer = 1,
#else
        .double_buffer = 0,
#endif
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        }
    };
    cfg.lvgl_port_cfg.task_stack = 6*1024;

    if (button_gpio_get_key_level((void *)BSP_LCD_SELECT)) {
        cfg.buffer_size = BSP_LCD_1_2_8_H_RES * BSP_LCD_1_2_8_V_RES / 4;
        cfg.double_buffer = 1;
    } else {
        cfg.buffer_size = BSP_LCD_0_9_6_H_RES * BSP_LCD_0_9_6_V_RES;
    }

    return bsp_display_start_with_config(&cfg);
}

lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    lv_disp_t *disp;
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg));
    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);
    // BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(disp), NULL);

    return disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

void bsp_display_rotate(lv_disp_t *disp, lv_disp_rot_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}

esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size)
{
    esp_err_t ret = ESP_OK;
    if ((btn_array_size < BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM) ||
            (btn_array == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (btn_cnt) {
        *btn_cnt = 0;
    }
    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        btn_array[i] = iot_button_create(&bsp_button_config[i]);
        if (btn_array[i] == NULL) {
            ret = ESP_FAIL;
            break;
        }
        if (btn_cnt) {
            (*btn_cnt)++;
        }
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    /* Initialize ADC and get ADC handle */
    BSP_ERROR_CHECK_RETURN_NULL(bsp_adc_initialize());
    bsp_adc_handle = bsp_adc_get_handle();
#endif

    for (int i = BSP_ADC_BUTTON_PREV; i < BSP_ADC_BUTTON_NUM; i++) {
        btn_array[BSP_BUTTON_NUM + i] = iot_button_create(&bsp_adc_button_config[i]);
        if (btn_array[BSP_BUTTON_NUM + i] == NULL) {
            ret = ESP_FAIL;
            break;
        }
        if (btn_cnt) {
            (*btn_cnt)++;
        }
    }
    return ret;
}
