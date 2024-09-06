/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "motor.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "motor";

static uint16_t s_motor_freq = 20;
static uint8_t s_motor_runtime = 5;
static uint8_t s_motor_state = MOTOR_PAUSED;
// task handle
static TaskHandle_t motor_task_handle = NULL;
static SemaphoreHandle_t motor_param_semaphore = NULL;

esp_err_t motor_driver_init(gpio_num_t motor_pin_a, gpio_num_t motor_pin_b)
{
    esp_err_t ret = ESP_OK;
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = MOTOR_LEDC_MODE,
        .timer_num        = MOTOR_LEDC_TIMER,
        .duty_resolution  = MOTOR_LEDC_DUTY_RES,
        .freq_hz          = MOTOR_LEDC_FREQUENCY, 
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "motor ledc timer config failed with error 0x%x", ret);
        return ret;
    }
    ledc_channel_config_t ledc_channel_motor_a = {
        .speed_mode     = MOTOR_LEDC_MODE,
        .channel        = MOTOR_LEDC_CHANNEL_A,
        .timer_sel      = MOTOR_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = motor_pin_a,
        .duty           = 0, 
        .hpoint         = 0
    };
    ret = ledc_channel_config(&ledc_channel_motor_a);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "motor driver input terminal A ledc channel initialization failed, error code: 0x%x", ret);
        return ret;
    }
    ledc_channel_config_t ledc_channel_motor_b = {
        .speed_mode     = MOTOR_LEDC_MODE,
        .channel        = MOTOR_LEDC_CHANNEL_B,
        .timer_sel      = MOTOR_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = motor_pin_b,
        .duty           = 0, 
        .hpoint         = 0
    };
    ret = ledc_channel_config(&ledc_channel_motor_b);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "motor driver input terminal B ledc channel initialization failed, error code: 0x%x", ret);
        return ret;
    }

    motor_param_semaphore = xSemaphoreCreateBinary();
    if (motor_param_semaphore == NULL) {
        ESP_LOGE(TAG, "create motor param_semaphore failed");
        ret = ESP_FAIL;
        return ret;
    }
    return ESP_OK;
}

/**
 * @brief   set pwm duty
 * @param   duty_ina duty of input terminal A
 * @param   duty_inb duty of input terminal B
 */
static void set_pwm_duty(int duty_ina, int duty_inb)
{
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL_A, duty_ina);
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL_B, duty_inb);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL_A);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL_B);
}

/**
 * @brief   set pwm state
 * @param   state state of pwm
 * @note    This function is used to set the state of the pwm. 
 *          It will set the duty of the input terminals based on the state.
 */
static void set_pwm_state(uint8_t state)
{
    switch (state) {
        case MOTOR_WAITING:     // waiting state
            set_pwm_duty(0, 0);
            break;
        case MOTOR_FORWARD:     // forward state
            set_pwm_duty(MOTOR_LEDC_DUTY_MAX, 0);
            break;
        case MOTOR_BACKWARD:    // backward state
            set_pwm_duty(0, MOTOR_LEDC_DUTY_MAX);
            break;
        case MOTOR_STOP:        // stop state
            set_pwm_duty(MOTOR_LEDC_DUTY_MAX, MOTOR_LEDC_DUTY_MAX);
            break;
        default:                // invalid state
            break;
    }
}

/**
 * @brief calculate vibration parameters 
 * @param frequency_hz frequency of vibration (Hz)
 * @param vibration_duration_ms duration of vibration (ms)
 * @param run_time pointer to store vibration time (ms)
 * @param wait_time pointer to store wait time (ms)
 */
static void calculate_vibration_params(uint8_t frequency_hz, uint8_t vibration_duration_ms, uint8_t *run_time, uint8_t *wait_time)
{
    // vibration period (ms)
    uint8_t period_ms = (uint8_t) (1000 / frequency_hz);
    if (period_ms < 2 * vibration_duration_ms) {
        ESP_LOGE(TAG, "vibration period is too short, please increase frequency or decrease vibration duration");
        *run_time = period_ms / 2;
        *wait_time = 0;
        return;
    }
    // vibration time (ms)
    *run_time = vibration_duration_ms;
    // wait time (ms)
    *wait_time = (uint8_t)((period_ms - (2 * vibration_duration_ms)) / 2);
}

/**
 * @brief motor vibration function 
 * @param run_time vibration time (ms)
 * @param wait_time wait time (ms)
 */
static void motor_vibration(uint8_t run_time, uint8_t wait_time)
{
    set_pwm_state(MOTOR_FORWARD);
    vTaskDelay(pdMS_TO_TICKS(run_time));
    set_pwm_state(MOTOR_STOP);
    vTaskDelay(pdMS_TO_TICKS(wait_time));
    set_pwm_state(MOTOR_BACKWARD);
    vTaskDelay(pdMS_TO_TICKS(run_time));
    set_pwm_state(MOTOR_STOP);
    vTaskDelay(pdMS_TO_TICKS(wait_time));
}

 /**
 * @brief   motor wait function
 * @note    This function is used to set the motor state to waiting. 
 *          It will set the duty of the input terminals to 0.
 */
static void motor_wait(void)
{
    set_pwm_state(MOTOR_WAITING);
}

/**
 * @brief   motor vibration task
 * @note    This task is used to generate vibration signal for the motor. 
 *          It will calculate the vibration parameters based on the frequency and runtime parameters, 
 *          and then generate the vibration signal by running the motor forward, backward, and stop.
 */
static void motor_vibration_task(void *pvParameters)
{
    uint8_t run_time, wait_time;
    calculate_vibration_params(s_motor_freq, s_motor_runtime, &run_time, &wait_time);
    while (1) {
        if (xSemaphoreTake(motor_param_semaphore, 0) == pdTRUE) {
            calculate_vibration_params(s_motor_freq, s_motor_runtime, &run_time, &wait_time);
        }
        motor_vibration(run_time, wait_time);
    }
    vTaskDelete(NULL);
}

void motor_start(void)
{
    if (motor_task_handle == NULL) {
        xTaskCreate(motor_vibration_task, "motor_vibration_task", 2048, NULL, 10, &motor_task_handle);
        s_motor_state = MOTOR_RUNNING;
    }
}

void motor_stop(void)
{
    if (motor_task_handle != NULL) {
        vTaskDelete(motor_task_handle);
        motor_task_handle = NULL;
        s_motor_state = MOTOR_PAUSED;
        motor_wait();
    }
}

void motor_set_params(motor_frequency_level_t frequency_level, motor_amplitude_level_t amplitude_level)
{
    switch (frequency_level) {
        case MOTOR_FREQUENCY_LEVEL_1:
            s_motor_freq = 100;
            break;
        case MOTOR_FREQUENCY_LEVEL_2:
            s_motor_freq = 125;
            break;
        case MOTOR_FREQUENCY_LEVEL_3:
            s_motor_freq = 250;
            break;
        case MOTOR_FREQUENCY_LEVEL_4:
            s_motor_freq = 500;
            break;
        default:
            s_motor_freq = 100;
            break;
    }
    switch (amplitude_level) {
        case MOTOR_AMPLITUDE_LEVEL_1:
            s_motor_runtime = 1;
            break;
        case MOTOR_AMPLITUDE_LEVEL_2:
            s_motor_runtime = 2;
            break;
        case MOTOR_AMPLITUDE_LEVEL_3:
            s_motor_runtime = 3;
            break;
        case MOTOR_AMPLITUDE_LEVEL_4:
            s_motor_runtime = 4;
            break;
        default:
            s_motor_runtime = 1;
            break;
    }
    if (motor_task_handle != NULL) {
        xSemaphoreGive(motor_param_semaphore);
    }
}

uint8_t motor_get_state(void)
{
    return s_motor_state;
}
