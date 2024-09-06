/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "buzzer.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "buzzer";

#define BUZZER_LEDC_TIMER              LEDC_TIMER_0
#define BUZZER_LEDC_MODE               LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_CHANNEL            LEDC_CHANNEL_0
#define BUZZER_LEDC_DUTY_RES           LEDC_TIMER_12_BIT // Set duty resolution to 12 bits
#define BUZZER_LEDC_DUTY               (4096) // Set duty to 100%. (2 ** 13) = 4096
#define BUZZER_LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz

/* start tone */
static const tone_t tones_start[] = {
    {NOTE_B5, MUSIC_EIGHTH_NOTE}, {0, MUSIC_SIXTEENTH_NOTE}, {NOTE_B5, MUSIC_EIGHTH_NOTE}, {0, MUSIC_SIXTEENTH_NOTE} // Start tone
};
static const int num_tones_start = sizeof(tones_start) / sizeof(tone_t);
/* end tone */
static const tone_t tones_end[] = {
    {NOTE_E5, MUSIC_EIGHTH_NOTE}, {0, MUSIC_SIXTEENTH_NOTE}, {NOTE_E5, MUSIC_EIGHTH_NOTE}, {0, MUSIC_SIXTEENTH_NOTE} // End tone
};
static const int num_tones_end = sizeof(tones_end) / sizeof(tone_t);
/* single press tone */ 
static const tone_t tones_single_press[] = {
    {NOTE_C5, MUSIC_EIGHTH_NOTE}, {0, MUSIC_SIXTEENTH_NOTE} // Short beeps
};
static const int num_tones_single_press = sizeof(tones_single_press) / sizeof(tone_t);
/* double press tone */ 
static const tone_t tones_double_press[] = {
    {NOTE_D5, MUSIC_EIGHTH_NOTE}, {0, MUSIC_SIXTEENTH_NOTE} // release tone
};
static const int num_tones_double_press = sizeof(tones_double_press) / sizeof(tone_t);
/* long press tone */ 
static const tone_t tones_long_press[] = {
    {NOTE_LA5, MUSIC_EIGHTH_NOTE}, {0, MUSIC_SIXTEENTH_NOTE} // Power off
};
static const int num_tones_long_press = sizeof(tones_long_press) / sizeof(tone_t);
/* error tone */ 
static const tone_t tones_error[] = {
    {NOTE_E4, MUSIC_EIGHTH_NOTE}, {0, MUSIC_SIXTEENTH_NOTE}, {NOTE_E4, MUSIC_EIGHTH_NOTE}, {0, MUSIC_SIXTEENTH_NOTE}  // Repeat
};
static const int num_tones_error = sizeof(tones_error) / sizeof(tone_t);

esp_err_t buzzer_init(gpio_num_t buzzer_pin)
{
    esp_err_t ret = ESP_OK;
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = BUZZER_LEDC_MODE,
        .timer_num        = BUZZER_LEDC_TIMER,
        .duty_resolution  = BUZZER_LEDC_DUTY_RES,
        .freq_hz          = BUZZER_LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "buzzer ledc timer config failed with error 0x%x", ret);
        return ret;
    }
    ledc_channel_config_t ledc_channel_buzzer = {
        .speed_mode     = BUZZER_LEDC_MODE,
        .channel        = BUZZER_LEDC_CHANNEL,
        .timer_sel      = BUZZER_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = buzzer_pin,
        .duty           = 0, 
        .hpoint         = 0
    };
    ret = ledc_channel_config(&ledc_channel_buzzer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "buzzer ledc channel initialization failed, error code: 0x%x", ret);
        return ret;
    }
    return ESP_OK;
}

/**
 * @brief   Play a tone with the given frequency and duration.
 *          If frequency is 0, the buzzer will be turned off.
 * @param frequency Frequency of the tone in Hz. If 0, the buzzer will be turned off.
 * @param duration Duration of the tone in milliseconds.
 */
static void buzzer_tone(int frequency, int duration)
{
    if (frequency != 0){
        ledc_set_freq(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, frequency);
        ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, duration);
    } else {
        ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    }
    
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    vTaskDelay(duration / portTICK_PERIOD_MS);
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

/**
 * @brief   Play a sequence of tones.
 * @param tones Pointer to an array of tones.
 * @param num_tones Number of tones in the array.
 */
static void buzzer_play_tones(const tone_t *tones, const int num_tones)
{
    for (int i = 0; i < num_tones; i++)
    {
        buzzer_tone(tones[i].frequency, tones[i].duration);    
    }
}

void buzzer_play_audio(audio_index_t audio_index)
{
    ESP_LOGD(TAG, "Playing audio %d", audio_index);

    switch (audio_index)
    {
    case AUDIO_INDEX_START:
        buzzer_play_tones(tones_start, num_tones_start);
        break;
    case AUDIO_INDEX_END:
        buzzer_play_tones(tones_end, num_tones_end);
        break;
    case AUDIO_INDEX_SINGLE_PRESS:
        buzzer_play_tones(tones_single_press, num_tones_single_press);
        break;
    case AUDIO_INDEX_DOUBLE_PRESS:
        buzzer_play_tones(tones_double_press, num_tones_double_press);
        break;
    case AUDIO_INDEX_LONG_PRESS:
        buzzer_play_tones(tones_long_press, num_tones_long_press);
        break;
    case AUDIO_INDEX_ERROR:
        buzzer_play_tones(tones_error, num_tones_error);
        break;
    default:
        ESP_LOGE(TAG, "Invalid audio index: %d", audio_index);
        break;
    }
    ESP_LOGD(TAG, "Finished playing audio %d", audio_index);
}


