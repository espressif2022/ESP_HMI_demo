/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "perf_test_main.h"

void test_perf_decoder_spiffs_lv(void)
{
    test_lvgl_add_disp();
    test_spiffs_fs_new();

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    test_performance_run(img, 0, "spiffs", "lv_sjpg", "C:/assets/navi_52.sjpg");
    test_performance_run(img, 0, "spiffs", "lv_spng", (const void *)"C:/assets/navi_52.png");

    test_lvgl_del_disp();
    test_spiffs_fs_del();
}
