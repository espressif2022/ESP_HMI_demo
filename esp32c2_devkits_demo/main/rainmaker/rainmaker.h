/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <esp_rmaker_core.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Rainmaker init
 * 
 */
esp_rmaker_device_t* rainmaker_init(esp_rmaker_device_write_cb_t write_cb);

/**
 * @brief Pass in the brushing mode character,return the corresponding number
 * 
 * @param brushing_mode brushing mode character
 * @return int8_t correspond number
 */
int8_t rainmaker_get_brushing_mode_num(char *brushing_mode);

/**
 * @brief Pass in the brushing mode number,return the corresponding character
 * 
 * @param brushing_mode brushing mode number
 * @return const char* corresponding character
 */
const char* rainmaker_get_brushing_mode_str(int brushing_mode);

/**
 * @brief get rainmaker connect status
 * 
 */
bool rainmaker_get_connect_status(void);

/**
 * @brief Wait for rainmaker connect
 * 
 */
void rainmaker_wait_connect(void);
#ifdef __cplusplus
}
#endif