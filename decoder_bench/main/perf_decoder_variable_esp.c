/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "perf_test_main.h"
#include "mmap_generate_Drive_A.h"
#include "esp_lv_sjpg.h"
#include "esp_lv_spng.h"

void test_perf_decoder_variable_esp(void)
{
    esp_lv_sjpg_decoder_handle_t sjpg_handle = NULL;
    esp_lv_spng_decoder_handle_t spng_handle = NULL;

    test_lvgl_add_disp();
    test_flash_fs_new();

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    static lv_img_dsc_t img_dsc;

    img_dsc.data_size = test_assets_get_size(MMAP_DRIVE_A_NAVI_52_JPG);
    img_dsc.data = test_assets_get_mem(MMAP_DRIVE_A_NAVI_52_JPG);
    test_performance_run(img, 0, "variable", "lv_sjpg", (const void *)&img_dsc);

    img_dsc.data_size = test_assets_get_size(MMAP_DRIVE_A_NAVI_52_PNG);
    img_dsc.data = test_assets_get_mem(MMAP_DRIVE_A_NAVI_52_PNG);
    test_performance_run(img, 0, "variable", "lv_spng", (const void *)&img_dsc);

    esp_lv_split_jpg_init(&sjpg_handle);
    esp_lv_split_png_init(&spng_handle);

    img_dsc.data_size = test_assets_get_size(MMAP_DRIVE_A_NAVI_52_JPG);
    img_dsc.data = test_assets_get_mem(MMAP_DRIVE_A_NAVI_52_JPG);
    test_performance_run(img, 0, "variable", "esp_lv_sjpg", (const void *)&img_dsc);

    img_dsc.data_size = test_assets_get_size(MMAP_DRIVE_A_NAVI_52_PNG);
    img_dsc.data = test_assets_get_mem(MMAP_DRIVE_A_NAVI_52_PNG);
    test_performance_run(img, 0, "variable", "esp_lv_spng", (const void *)&img_dsc);

    esp_lv_split_jpg_deinit(sjpg_handle);
    esp_lv_split_png_deinit(spng_handle);

    test_lvgl_del_disp();
}
