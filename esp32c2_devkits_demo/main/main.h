
/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "bsp/esp-bsp.h"
#include "iot_button.h"

void printf_stack();

esp_err_t app_btn_init(void);

esp_err_t app_btn_register_callback(bsp_button_t btn, button_event_t event, button_cb_t callback, void *user_data);

esp_err_t app_btn_rm_all_callback(bsp_button_t btn);

esp_err_t app_btn_rm_event_callback(bsp_button_t btn, size_t event);

esp_err_t app_btn_register_event_callback(bsp_button_t btn, button_event_config_t cfg, button_cb_t callback, void *user_data);