/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

#include "esp_lv_fs.h"
#include "esp_lv_spng.h"
#include "esp_lv_sqoi.h"

#include "mmap_generate_Drive_A.h"
#include "mmap_generate_Drive_B.h"

static const char *TAG = "1_28_ui";

LV_IMG_DECLARE(red_bg);
LV_IMG_DECLARE(yellow_bg);
LV_IMG_DECLARE(blue_bg);

static void btn_press_left_cb(void *handle, void *arg);

static void btn_press_OK_cb(void *handle, void *arg);

static void btn_press_right_cb(void *handle, void *arg);

static void image_mmap_init();

#define MAX_COUNTERS        3

typedef enum {
    THEME_SELECT_CHILD,
    THEME_SELECT_CLEAN,
    THEME_SELECT_QUICK,
    THEME_MAX_NUM,
} theme_select_t;

typedef struct {
    int64_t start;
    int64_t acc;
    char str1[15];
    char str2[15];
} PerfCounter;

static theme_select_t theme_select = THEME_SELECT_CHILD;
static bool anmi_do_run = true;

static mmap_assets_handle_t asset_DriverA_handle;
static esp_lv_fs_handle_t fs_DriverA_handle;

static mmap_assets_handle_t asset_DriverB_handle;
static esp_lv_fs_handle_t fs_DriverB_handle;

static esp_lv_spng_decoder_handle_t spng_decoder;
static esp_lv_sqoi_decoder_handle_t qoi_decoder;

static PerfCounter perf_counters[MAX_COUNTERS] = {0};

static void perfmon_start(int ctr, const char* fmt1, const char* fmt2, ...)
{
    va_list args;
    va_start(args, fmt2);
    vsnprintf(perf_counters[ctr].str1, sizeof(perf_counters[ctr].str1), fmt1, args);
    vsnprintf(perf_counters[ctr].str2, sizeof(perf_counters[ctr].str2), fmt2, args);
    va_end(args);

    perf_counters[ctr].start = esp_timer_get_time();
}

static void perfmon_end(int ctr, int count)
{
    int64_t time_diff = esp_timer_get_time() - perf_counters[ctr].start;
    float time_in_sec = (float)time_diff / 1000000;
    float frequency = count / time_in_sec;

    printf("Perf ctr[%d], [%8s][%8s]: %.2f FPS (%.2f ms)\n",
           ctr, perf_counters[ctr].str1, perf_counters[ctr].str2, frequency, time_in_sec * 1000 / count);
}

void ui_1_28_start()
{
    static lv_img_dsc_t img_dsc_motive;

    app_btn_register_callback(BSP_BUTTON_NUM + BSP_ADC_BUTTON_PREV, BUTTON_PRESS_UP, btn_press_left_cb, NULL);
    app_btn_register_callback(BSP_BUTTON_NUM + BSP_ADC_BUTTON_ENTER, BUTTON_PRESS_UP, btn_press_OK_cb, NULL);
    app_btn_register_callback(BSP_BUTTON_NUM + BSP_ADC_BUTTON_NEXT, BUTTON_PRESS_UP, btn_press_right_cb, NULL);

    image_mmap_init();
    esp_lv_split_png_init(&spng_decoder);
    esp_lv_split_qoi_init(&qoi_decoder);

    bsp_display_lock(0);

    ESP_LOGI(TAG, "screen size:[%d,%d]", LV_HOR_RES, LV_VER_RES);

    bsp_display_lock(0);

    lv_obj_t *obj_bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj_bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_align(obj_bg, LV_ALIGN_CENTER);
    lv_obj_clear_flag(obj_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(obj_bg, 0, 0);

    lv_obj_t *obj_img_run_particles = lv_img_create(obj_bg);
    lv_obj_set_align(obj_img_run_particles, LV_ALIGN_CENTER);

    bsp_display_unlock();

    theme_select_t theme_last = THEME_MAX_NUM;
    uint8_t list = 100;
    uint8_t img_ossfet = 0;
    int fps_count = 0;

    while (1) {
        bsp_display_lock(0);

        if (theme_last ^ theme_select) {
            theme_last = theme_select;
            if (THEME_SELECT_CHILD == theme_select) {
                img_ossfet = 0;
            } else if (THEME_SELECT_CLEAN == theme_select) {
                img_ossfet = 0;
            } else if (THEME_SELECT_QUICK == theme_select) {
                img_ossfet = 0;
            }
        }
        if (true == anmi_do_run) {
            list++;
            lv_obj_clear_flag(obj_img_run_particles, LV_OBJ_FLAG_HIDDEN);

#if 0
            img_dsc_motive.data_size = mmap_assets_get_size(asset_DriverA_handle, img_ossfet + (list) % MMAP_SPIFFS_ASSETS_FILES);
            img_dsc_motive.data = mmap_assets_get_mem(asset_DriverA_handle, img_ossfet + (list) % MMAP_SPIFFS_ASSETS_FILES);
            lv_img_set_src(obj_img_run_particles, &img_dsc_motive);
#else
            char path[30];
            if(theme_select%2){
                sprintf(path, "A:%s", mmap_assets_get_name(asset_DriverA_handle, img_ossfet + (list) % MMAP_DRIVE_A_FILES));
            } else {
                sprintf(path, "B:%s", mmap_assets_get_name(asset_DriverB_handle, img_ossfet + (list) % MMAP_DRIVE_B_FILES));
            }
            ESP_LOGI("PATH", "%s", path);
            lv_img_set_src(obj_img_run_particles, path);
#endif

            if (fps_count % 10 == 0) {
                perfmon_start(0, "PFS", "png");
                // printf_stack();
            } else if (fps_count % 10 == 9) {
                perfmon_end(0, 10);
            }
            fps_count++;
        } else {
            list = 0;
            lv_obj_add_flag(obj_img_run_particles, LV_OBJ_FLAG_HIDDEN);
        }
        lv_refr_now(NULL);
        bsp_display_unlock();

        // vTaskDelay(pdMS_TO_TICKS(1));
    }

    mmap_assets_del(asset_DriverA_handle);
    mmap_assets_del(asset_DriverB_handle);
    esp_lv_split_png_deinit(spng_decoder);
}

static void btn_press_left_cb(void *handle, void *arg)
{
    theme_select = (theme_select + 2) % THEME_MAX_NUM;
    anmi_do_run = true;
    ESP_LOGI("BTN", "left:%d", theme_select);
}

static void btn_press_OK_cb(void *handle, void *arg)
{
    anmi_do_run = !anmi_do_run;
    ESP_LOGI("BTN", "OK:%d", anmi_do_run);
}

static void btn_press_right_cb(void *handle, void *arg)
{
    anmi_do_run = true;
    theme_select = (theme_select + 1) % THEME_MAX_NUM;
    ESP_LOGI("BTN", "right:%d", theme_select);
}

static void image_mmap_init()
{
    const mmap_assets_config_t config_DriveA = {
        .partition_label = "assets_A",
        .max_files = MMAP_DRIVE_A_FILES,
        .checksum = MMAP_DRIVE_A_CHECKSUM,
        .flags = {
            .app_bin_check = true,
            .mmap_enable = true,
            // .mmap_enable = false,
        }
    };
    ESP_ERROR_CHECK(mmap_assets_new(&config_DriveA, &asset_DriverA_handle));

    const fs_cfg_t fs_cfg_a = {
        .fs_letter = 'A',
        .fs_assets = asset_DriverA_handle,
        .fs_nums = MMAP_DRIVE_A_FILES
    };
    esp_lv_fs_desc_init(&fs_cfg_a, &fs_DriverA_handle);


    const mmap_assets_config_t config_DriveB = {
        .partition_label = "assets_B",
        .max_files = MMAP_DRIVE_B_FILES,
        .checksum = MMAP_DRIVE_B_CHECKSUM,
        .flags = {
            .app_bin_check = true,
            .mmap_enable = true,
            // .mmap_enable = false,
        }
    };
    ESP_ERROR_CHECK(mmap_assets_new(&config_DriveB, &asset_DriverB_handle));

    const fs_cfg_t fs_cfg_b = {
        .fs_letter = 'B',
        .fs_assets = asset_DriverB_handle,
        .fs_nums = MMAP_DRIVE_B_FILES
    };
    esp_lv_fs_desc_init(&fs_cfg_b, &fs_DriverB_handle);
}