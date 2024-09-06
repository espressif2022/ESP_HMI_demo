/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_spiffs.h"
#include "esp_timer.h"

#include "esp_lv_fs.h"
#include "esp_lv_sjpg.h"
#include "esp_lv_spng.h"

#include "perf_test_main.h"

#include "mmap_generate_Drive_A.h"
#include "mmap_generate_Drive_B.h"

// #include "bsp/display.h"

static const char *TAG = "perf_decoder";

#define TEST_LCD_H_RES      110
#define TEST_LCD_V_RES      110
#define MAX_COUNTERS        3
#define TEST_COUNTERS       10

static lv_disp_drv_t *lv_disp_drv = NULL;
static lv_disp_draw_buf_t *lv_disp_buf = NULL;
static SemaphoreHandle_t lv_flush_sync_sem;

static mmap_assets_handle_t mmap_drive_a_handle;
static mmap_assets_handle_t mmap_drive_b_handle;

static esp_lv_fs_handle_t fs_drive_a_handle;
static esp_lv_fs_handle_t fs_drive_b_handle;

typedef struct {
    int64_t start;
    int64_t acc;
    char str1[15];
    char str2[15];
} PerfCounter;

static PerfCounter perf_counters[MAX_COUNTERS] = {0};

static void perfmon_start(int ctr, const char* fmt1, const char* fmt2, ...)
{
    va_list args;
    va_start(args, fmt2);
    vsnprintf(perf_counters[ctr].str1, sizeof(perf_counters[ctr].str1), fmt1, args);
    vsnprintf(perf_counters[ctr].str2, sizeof(perf_counters[ctr].str2), fmt2, args);
    va_end(args);

    // perf_counters[ctr].start = esp_log_timestamp();
    perf_counters[ctr].start = esp_timer_get_time();
}

static void perfmon_end(int ctr, int count)
{
    // perf_counters[ctr].acc = esp_log_timestamp() - perf_counters[ctr].start;
    perf_counters[ctr].acc = esp_timer_get_time() - perf_counters[ctr].start;
    printf("Perf ctr[%d], [%15s][%15s]: %.2f ms\n",
           ctr, perf_counters[ctr].str1, perf_counters[ctr].str2, ((float)perf_counters[ctr].acc / count) / 1000);
}

esp_err_t test_mmap_drive_new(void)
{
    const mmap_assets_config_t asset_cfg_a = {
        .partition_label = "assets_A",
        .max_files = MMAP_DRIVE_A_FILES,
        .checksum = MMAP_DRIVE_A_CHECKSUM,
        .flags = {.mmap_enable = true}
    };
    mmap_assets_new(&asset_cfg_a, &mmap_drive_a_handle);

    const mmap_assets_config_t asset_cfg_b = {
        .partition_label = "assets_B",
        .max_files = MMAP_DRIVE_B_FILES,
        .checksum = MMAP_DRIVE_B_CHECKSUM,
        .flags = {.mmap_enable = false}
    };
    mmap_assets_new(&asset_cfg_b, &mmap_drive_b_handle);

    return ESP_OK;
}

esp_err_t test_mmap_drive_del(void)
{
    mmap_assets_del(mmap_drive_a_handle);
    mmap_assets_del(mmap_drive_b_handle);

    return ESP_OK;
}

esp_err_t test_spiffs_fs_new(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/assets",
        .partition_label = "assets_C",
        .max_files = 5,
    };

    return esp_vfs_spiffs_register(&conf);
}

esp_err_t test_spiffs_fs_del(void)
{
    return esp_vfs_spiffs_unregister("assets");
}

void test_flash_fs_new(void)
{
    const fs_cfg_t fs_cfg_a = {
        .fs_letter = 'A',
        .fs_assets = mmap_drive_a_handle,
        .fs_nums = MMAP_DRIVE_A_FILES
    };
    esp_lv_fs_desc_init(&fs_cfg_a, &fs_drive_a_handle);

    const fs_cfg_t fs_cfg_b = {
        .fs_letter = 'B',
        .fs_assets = mmap_drive_b_handle,
        .fs_nums = MMAP_DRIVE_B_FILES
    };
    esp_lv_fs_desc_init(&fs_cfg_b, &fs_drive_b_handle);
}

void test_flash_fs_del(void)
{
    esp_lv_fs_desc_deinit(fs_drive_a_handle);
    esp_lv_fs_desc_deinit(fs_drive_b_handle);
}

const uint8_t *test_assets_get_mem(int index)
{
    return mmap_assets_get_mem(mmap_drive_a_handle, index);
}

int test_assets_get_size(int index)
{
    return mmap_assets_get_size(mmap_drive_a_handle, index);
}

static void test_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    if (color_map[0].full != 0xF7BE && color_map[0].full != 0xFFFF) {
        xSemaphoreGive(lv_flush_sync_sem);
    }

    // bsp_flush_callback(area->x1, area->y1, area->x2, area->y2, (uint8_t *)color_map);
    lv_disp_flush_ready(drv);
}

void test_performance_run(lv_obj_t *img, int ctr, const char *str1, const char *str2, const void *img_src)
{
    lv_img_set_src(img, NULL);
    lv_refr_now(NULL);

    perfmon_start(ctr, str1, str2);
    for (int i = 0; i < TEST_COUNTERS; i++) {
        lv_img_set_src(img, (lv_img_dsc_t *)img_src);
        lv_refr_now(NULL);
        if (xSemaphoreTake(lv_flush_sync_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
            ESP_LOGE(TAG, "[%s:%d]decoder failed", __FILE__, __LINE__);
        }
    }
    perfmon_end(ctr, TEST_COUNTERS);
}

void test_lvgl_add_disp()
{
    lv_init();

    lv_disp_buf = heap_caps_malloc(sizeof(lv_disp_draw_buf_t), MALLOC_CAP_DEFAULT);
    assert(lv_disp_buf);

    uint32_t buffer_size = TEST_LCD_H_RES * TEST_LCD_V_RES;
    lv_color_t *buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DEFAULT);
    assert(buf1);
    lv_disp_draw_buf_init(lv_disp_buf, buf1, NULL, buffer_size);

    lv_disp_drv = heap_caps_malloc(sizeof(lv_disp_drv_t), MALLOC_CAP_DEFAULT);
    assert(lv_disp_drv);
    lv_disp_drv_init(lv_disp_drv);
    (lv_disp_drv)->hor_res = TEST_LCD_H_RES;
    (lv_disp_drv)->ver_res = TEST_LCD_V_RES;
    (lv_disp_drv)->flush_cb = test_flush_callback;
    (lv_disp_drv)->draw_buf = lv_disp_buf;
    lv_disp_drv_register(lv_disp_drv);

    lv_flush_sync_sem = xSemaphoreCreateBinary();
    assert(lv_flush_sync_sem);
}

void test_lvgl_del_disp()
{
    free(lv_disp_drv->draw_buf->buf1);
    free(lv_disp_drv->draw_buf);
    free(lv_disp_drv);
#if LV_ENABLE_GC || !LV_MEM_CUSTOM
    /* Deinitialize LVGL */
    lv_deinit();
#endif
    vSemaphoreDelete(lv_flush_sync_sem);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Perf ctr target: %s", CONFIG_IDF_TARGET);

    // bsp_display_lcd_start();
    // bsp_display_backlight_on();

    test_mmap_drive_new();

    test_perf_decoder_variable_esp();

    test_perf_decoder_fs_lv();
    test_perf_decoder_fs_esp();

    test_perf_decoder_spiffs_lv();
    test_perf_decoder_spiffs_esp();

    test_mmap_drive_del();
}
