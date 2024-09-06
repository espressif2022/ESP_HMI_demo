/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "perf_test_main.h"

void test_perf_decoder_fs_lv(void)
{
    test_lvgl_add_disp();
    test_flash_fs_new();

    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_obj_set_align(img, LV_ALIGN_TOP_LEFT);

    test_performance_run(img, 0, "mmap_enable", "lv_sjpg", (const void *)"A:navi_52.jpg");
    test_performance_run(img, 0, "mmap_enable", "lv_spng", (const void *)"A:navi_52.png");
    test_performance_run(img, 0, "mmap_disable", "lv_sjpg", (const void *)"B:navi_52.jpg");
    test_performance_run(img, 0, "mmap_disable", "lv_spng", (const void *)"B:navi_52.png");

    test_lvgl_del_disp();
    test_flash_fs_del();
}
