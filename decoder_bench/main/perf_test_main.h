/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

#include "lvgl.h"

#include "esp_err.h"

/**
 * @brief Create and initialize a new memory-mapped drive.
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_* error codes on failure
 */
esp_err_t test_mmap_drive_new(void);

/**
 * @brief Delete and clean up a memory-mapped drive.
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_* error codes on failure
 */
esp_err_t test_mmap_drive_del(void);

/**
 * @brief Create and initialize a new SPIFFS filesystem.
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_* error codes on failure
 */
esp_err_t test_spiffs_fs_new(void);

/**
 * @brief Delete and clean up a SPIFFS filesystem.
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_* error codes on failure
 */
esp_err_t test_spiffs_fs_del(void);

/**
 * @brief Create and initialize a new flash filesystem.
 */
void test_flash_fs_new(void);

/**
 * @brief Delete and clean up a flash filesystem.
 */
void test_flash_fs_del(void);

/**
 * @brief Get a pointer to an asset in memory by its index.
 *
 * @param index The index of the asset.
 * @return Pointer to the asset in memory.
 */
const uint8_t *test_assets_get_mem(int index);

/**
 * @brief Get the size of an asset by its index.
 *
 * @param index The index of the asset.
 * @return Size of the asset.
 */
int test_assets_get_size(int index);

/**
 * @brief Add a new display to the LVGL (Light and Versatile Graphics Library).
 */
void test_lvgl_add_disp();

/**
 * @brief Remove a display from the LVGL (Light and Versatile Graphics Library).
 */
void test_lvgl_del_disp();

/**
 * @brief Test performance of the ESP decoder with variable settings.
 */
void test_perf_decoder_variable_esp(void);

/**
 * @brief Test performance of the LVGL decoder with filesystem-based settings.
 */
void test_perf_decoder_fs_lv(void);

/**
 * @brief Test performance of the ESP decoder with filesystem-based settings.
 */
void test_perf_decoder_fs_esp(void);

/**
 * @brief Test performance of the LVGL decoder with SPIFFS settings.
 */
void test_perf_decoder_spiffs_lv(void);

/**
 * @brief Test performance of the ESP decoder with SPIFFS settings.
 */
void test_perf_decoder_spiffs_esp(void);

/**
 * @brief Run a performance test on an LVGL image object.
 *
 * This function starts the performance counter, sets the image source,
 * refreshes the display, waits for the result, and then ends the performance
 * counter and prints the results. The process is repeated 10 times.
 *
 * @param img The LVGL image object to be tested.
 * @param ctr The performance counter index.
 * @param str1 The first string identifier for the performance counter.
 * @param str2 The second string identifier for the performance counter.
 * @param img_src The source path of the image to be set on the LVGL image object.
 */
void test_performance_run(lv_obj_t *img, int ctr, const char *str1, const char *str2, const void *img_src);
