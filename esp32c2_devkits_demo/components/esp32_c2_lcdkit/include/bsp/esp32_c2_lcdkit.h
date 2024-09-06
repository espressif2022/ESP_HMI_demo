/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2s_pdm.h"
#include "iot_button.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "bsp/display.h"

/**************************************************************************************************
 *  ESP32-C3-LCDkit pinout
 **************************************************************************************************/

/* Display */
#define BSP_LCD_DATA0           (GPIO_NUM_0)
#define BSP_LCD_PCLK            (GPIO_NUM_1)
#define BSP_LCD_CS              (GPIO_NUM_2)
#define BSP_LCD_DC              (GPIO_NUM_3)
#define BSP_LCD_RST             (GPIO_NUM_10)
#define BSP_LCD_BACKLIGHT       (GPIO_NUM_NC)

#define BSP_LCD_SELECT          (GPIO_NUM_5)

/* Light */
#define BSP_RGB_CTRL            (GPIO_NUM_NC)

/* Buttons */
/* Buttons */
typedef enum {
    BSP_BUTTON_CONFIG,
    BSP_BUTTON_NUM
} bsp_button_t;

/* ADC Buttons */
typedef enum {
    BSP_ADC_BUTTON_PREV,
    BSP_ADC_BUTTON_ENTER,
    BSP_ADC_BUTTON_NEXT,
    BSP_ADC_BUTTON_NUM
} bsp_adc_button_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP display configuration structure
 *
 */
typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;  /*!< LVGL port configuration */
    uint32_t        buffer_size;    /*!< Size of the buffer for the screen in pixels */
    bool            double_buffer;  /*!< True, if should be allocated two buffers */
    struct {
        unsigned int buff_dma: 1;    /*!< Allocated LVGL buffer will be DMA capable */
        unsigned int buff_spiram: 1; /*!< Allocated LVGL buffer will be in PSRAM */
    } flags;
} bsp_display_cfg_t;

/**************************************************************************************************
 *
 * LCD interface
 *
 * ESP32-C3-LCDkit is shipped with 1.28inch GC9A01 display controller.
 * It features 16-bit colors, 240x240 resolution.
 *
 * LVGL is used as graphics library. LVGL is NOT thread safe, therefore the user must take LVGL mutex
 * by calling bsp_display_lock() before calling and LVGL API (lv_...) and then give the mutex with
 * bsp_display_unlock().
 *
 * Display's backlight must be enabled explicitly by calling bsp_display_backlight_on()
 **************************************************************************************************/
#define BSP_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)
#define BSP_LCD_SPI_NUM            (SPI2_HOST)

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_disp_t *bsp_display_start(void);

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @param cfg display configuration
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);


/**
 * @brief Get pointer to input device (touch, buttons, ...)
 *
 * @note The LVGL input device is initialized in bsp_display_start() function.
 *
 * @return Pointer to LVGL input device or NULL when not initialized
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. 0 will block indefinitely.
 * @return true  Mutex was taken
 * @return false Mutex was NOT taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_display_unlock(void);


/**************************************************************************************************
 *
 * ADC interface
 *
 * There are multiple devices connected to ADC peripheral:
 *  - Buttons
 *
 * After initialization of ADC, use adc_handle when using ADC driver.
 **************************************************************************************************/

#define BSP_ADC_UNIT     ADC_UNIT_1

/**
 * @brief Initialize ADC
 *
 * The ADC can be initialized inside BSP, when needed.
 *
 * @param[out] adc_handle Returned ADC handle
 */
esp_err_t bsp_adc_initialize(void);


#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
/**
 * @brief Get ADC handle
 *
 * @note This function is available only in IDF5 and higher
 *
 * @return ADC handle
 */
adc_oneshot_unit_handle_t bsp_adc_get_handle(void);
#endif

/**
 * @brief Initialize all buttons
 *
 * Returned button handlers must be used with espressif/button component API
 *
 * @note ADC buttons BSP_BUTTON_PREV, BSP_BUTTON_ENTER, BSP_BUTTON_NEXT won't be created by this API,
 *       they will be maintained by esp_lvgl_port.
 *
 * @param[out] btn_array      Output button array
 * @param[out] btn_cnt        Number of button handlers saved to btn_array, can be NULL
 * @param[in]  btn_array_size Size of output button array. Must be at least BSP_BUTTON_NUM
 * @return
 *     - ESP_OK               All buttons initialized
 *     - ESP_ERR_INVALID_ARG  btn_array is too small or NULL
 *     - ESP_FAIL             Underlying iot_button_create failed
 */
esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size);

/**
 * @brief Rotate screen
 *
 * Display must be already initialized by calling bsp_display_start()
 *
 * @param[in] disp Pointer to LVGL display
 * @param[in] rotation Angle of the display rotation
 */
void bsp_display_rotate(lv_disp_t *disp, lv_disp_rot_t rotation);

/**************************************************************************************************
 *
 * SPIFFS
 *
 * After mounting the SPIFFS, it can be accessed with stdio functions ie.:
 * \code{.c}
 * FILE* f = fopen(BSP_SPIFFS_MOUNT_POINT"/hello.txt", "w");
 * fprintf(f, "Hello World!\n");
 * fclose(f);
 * \endcode
 **************************************************************************************************/
#define BSP_SPIFFS_MOUNT_POINT      CONFIG_BSP_SPIFFS_MOUNT_POINT

/**
 * @brief Mount SPIFFS to virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_spiffs_register was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - ESP_FAIL if partition can not be mounted
 *      - other error codes
 */
esp_err_t bsp_spiffs_mount(void);

/**
 * @brief Unmount SPIFFS from virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_FOUND if the partition table does not contain SPIFFS partition with given label
 *      - ESP_ERR_INVALID_STATE if esp_vfs_spiffs_unregister was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - ESP_FAIL if partition can not be mounted
 *      - other error codes
 */
esp_err_t bsp_spiffs_unmount(void);

#ifdef __cplusplus
}
#endif
