/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "perf_test_main.h"
#include "esp_lv_fs.h"
#include "esp_lv_sjpg.h"
#include "esp_lv_spng.h"
#include "esp_lv_sqoi.h"

void test_perf_decoder_fs_esp(void)
{
    esp_lv_sjpg_decoder_handle_t sjpg_handle = NULL;
    esp_lv_spng_decoder_handle_t spng_handle = NULL;
    esp_lv_sqoi_decoder_handle_t sqoi_decoder = NULL;

    test_lvgl_add_disp();
    test_flash_fs_new();

    esp_lv_split_jpg_init(&sjpg_handle);
    esp_lv_split_png_init(&spng_handle);
    esp_lv_split_qoi_init(&sqoi_decoder);

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    test_performance_run(img, 0, "mmap_enable", "esp_lv_sjpg", (const void *)"A:navi_52.jpg");
    test_performance_run(img, 0, "mmap_disable", "esp_lv_sjpg", (const void *)"B:navi_52.jpg");
    test_performance_run(img, 0, "mmap_enable", "esp_lv_spng", (const void *)"A:navi_52.png");
    test_performance_run(img, 0, "mmap_disable", "esp_lv_spng", (const void *)"B:navi_52.png");
    test_performance_run(img, 0, "mmap_enable", "esp_lv_sqoi", (const void *)"A:navi_52.qoi");
    test_performance_run(img, 0, "mmap_disable", "esp_lv_sqoi", (const void *)"B:navi_52.qoi");

    esp_lv_split_jpg_deinit(sjpg_handle);
    esp_lv_split_png_deinit(spng_handle);
    esp_lv_split_qoi_deinit(sqoi_decoder);

    test_lvgl_del_disp();
    test_flash_fs_del();
}
