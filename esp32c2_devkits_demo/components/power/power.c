/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"

#include "power.h"

static const char *TAG = "power";

adc_oneshot_unit_handle_t power_adc_handle = NULL;
adc_cali_handle_t power_adc_cali_handle = NULL;
bool do_power_calibration = false;

static int adc_raw[1] = {0};
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void adc_calibration_deinit(adc_cali_handle_t handle);

/**
 * @brief   Power ADC Init
 * @note    This function is used to initialize the power ADC.
 */
void power_adc_init(void)
{
    //-------------ADC1 Init---------------//
    
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &power_adc_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(power_adc_handle, POWER_ADC_CHANNEL, &config));

    //-------------ADC1 Calibration Init---------------//
    
    do_power_calibration = adc_calibration_init(ADC_UNIT_1, POWER_ADC_CHANNEL, ADC_ATTEN_DB_12, &power_adc_cali_handle);

}

void power_init(void)
{
    // power_adc_init();
}

/**
 * @brief   Get Power Voltage
 * @note    This function is used to get the power voltage.
 *          It reads the ADC1 channel 4 and performs the ADC calibration if necessary.
 * @return  The power voltage.
 */
static int get_power_voltage(void)
{
    ESP_ERROR_CHECK(adc_oneshot_read(power_adc_handle, POWER_ADC_CHANNEL, &adc_raw[0]));
    int voltage[1] = {0};
    if (do_power_calibration) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(power_adc_cali_handle, adc_raw[0], &voltage[0]));
    }
    return voltage[0];
}

int get_power_value(void)
{
    int power_voltage = 0;
    int power_value = 0;
    // power_voltage = POWER_VOLTAGE_RATIO * get_power_voltage();
    ESP_LOGD(TAG, "Power Voltage: %d mV", power_voltage);
    if (power_voltage > 4.1 * 1000) {   
        power_value = 100;
    } else if (power_voltage > 3.1 * 1000) {
        power_value = 100.0f * (power_voltage - 3.1 * 1000) / (4.1 * 1000 - 3.1 * 1000);
    } else {
        power_value = 0;
    }
    return 90;
}

/**
 * @brief   ADC Calibration Init
 * @note    This function is used to initialize the ADC calibration.
 * @param   unit       ADC unit
 * @param   channel    ADC channel
 * @param   atten      ADC attenuation
 * @param   out_handle Pointer to store the calibration handle
 * @return  True if the calibration is successful, otherwise false.
 */
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            // .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

/**
 * @brief   ADC Calibration Deinit
 * @note    This function is used to deinitialize the ADC calibration. 
 * @param   handle     ADC calibration handle
 */
__attribute__((unused)) static void adc_calibration_deinit(adc_cali_handle_t handle) 
{
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
}

