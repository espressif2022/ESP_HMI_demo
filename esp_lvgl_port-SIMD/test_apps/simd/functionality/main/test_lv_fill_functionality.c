/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lv_fill_common.h"
#include "lv_draw_sw_blend.h"
#include "lv_draw_sw_blend_to_argb8888.h"
#include "lv_draw_sw_blend_to_rgb565.h"

// ------------------------------------------------- Defines -----------------------------------------------------------

#define DBG_PRINT_OUTPUT false
#define CANARY_BYTES 4

// ------------------------------------------------- Macros and Types --------------------------------------------------

#define UPDATE_TEST_CASE(test_case_ptr, dest_w, dest_h, dest_stride, unalign_byte) ({       \
    (test_case_ptr)->active_buf_len = (size_t)(dest_h * dest_stride);                       \
    (test_case_ptr)->total_buf_len = (size_t)((dest_h * dest_stride) + (CANARY_BYTES * 2)); \
    (test_case_ptr)->dest_w = (dest_w);             \
    (test_case_ptr)->dest_h = (dest_h);             \
    (test_case_ptr)->dest_stride = (dest_stride);   \
    (test_case_ptr)->unalign_byte = (unalign_byte); \
})

static const char *TAG_LV_FILL_FUNC = "LV Fill Functionality";
static char test_msg_buf[128];

static lv_color_t test_color = {
    .blue = 0x56,
    .green = 0x34,
    .red = 0x12,
};

// ------------------------------------------------ Static function headers --------------------------------------------

/**
 * @brief Generate all the functionality test combinations
 *
 * - generate functionality test combinations, based on the provided test_matrix struct
 *
 * @param[in] test_matrix Pointer to structure defining test matrix - all the test combinations
 * @param[in] test_case Pointer ot structure defining functionality test case
 */
static void functionality_test_matrix(test_matrix_params_t *test_matrix, test_case_params_t *test_case, bool use_asm);

/**
 * @brief Fill test buffers for functionality test
 *
 * @param[in] test_case Pointer ot structure defining functionality test case
 */
static void fill_test_bufs(test_case_params_t *test_case);

/**
 * @brief The actual functionality test
 *
 * - function prepares structures for functionality testing and runs the LVGL API
 *
 * @param[in] test_case Pointer ot structure defining functionality test case
 */
static void lv_fill_functionality(test_case_params_t *test_case, bool use_asm);

/**
 * @brief Evaluate results for 32bit data length
 *
 * @param[in] test_case Pointer ot structure defining functionality test case
 */
static void test_eval_32bit_data(test_case_params_t *test_case, bool use_asm);

/**
 * @brief Evaluate results for 16bit data length
 *
 * @param[in] test_case Pointer ot structure defining functionality test case
 */
static void test_eval_16bit_data(test_case_params_t *test_case, bool use_asm);

#define MAX_COUNTERS        3

typedef struct {
    int64_t start;
    int64_t acc;
    char str1[15];
    char str2[15];
} PerfCounter;

static PerfCounter perf_counters[MAX_COUNTERS] = {0};

static void perfmon_start(int ctr, const char* fmt1, const char* fmt2, ...)
{
    va_list args;
    va_start(args, fmt2);
    vsnprintf(perf_counters[ctr].str1, sizeof(perf_counters[ctr].str1), fmt1, args);
    vsnprintf(perf_counters[ctr].str2, sizeof(perf_counters[ctr].str2), fmt2, args);
    va_end(args);

    perf_counters[ctr].start = esp_timer_get_time();
}

static void perfmon_end(int ctr, int count)
{
    perf_counters[ctr].acc = esp_timer_get_time() - perf_counters[ctr].start;
    printf("Perf ctr[%d], [%15s][%15s]: %.2f ms\n",
           ctr, perf_counters[ctr].str1, perf_counters[ctr].str2, ((float)perf_counters[ctr].acc / count) / 1000);
    perf_counters[ctr].start = 0;
}

// ------------------------------------------------ Test cases ---------------------------------------------------------

/*
Functionality tests

Purpose:
    - Test that an assembly version of LVGL blending API achieves the same results as the ANSI version

Procedure:
    - Prepare testing matrix, to cover all the possible combinations of destination array widths, lengths, memory alignment...
    - Run assembly version of the LVGL blending API
    - Run ANSI C version of the LVGL blending API
    - Compare the results
    - Repeat above 3 steps for each test matrix setup
*/

// ------------------------------------------------ Test cases stages --------------------------------------------------

TEST_CASE("Test fill functionality ARGB8888", "[fill][functionality][ARGB8888]")
{
    test_matrix_params_t test_matrix = {
        .min_w = 8,             // 8 is the lower limit for the esp32s3 asm implementation, otherwise esp32 is executed
        .min_h = 1,
        .max_w = 16,
        .max_h = 64,
        .min_unalign_byte = 0,
        .max_unalign_byte = 16,
        .unalign_step = 1,
        .dest_stride_step = 1,
        .test_combinations_count = 0,
    };

    test_case_params_t test_case = {
        .blend_api_func = &lv_draw_sw_blend_color_to_argb8888,
        .color_format = LV_COLOR_FORMAT_ARGB8888,
        .data_type_size = sizeof(uint32_t),
    };

    ESP_LOGI(TAG_LV_FILL_FUNC, "running test for ARGB8888 color format:asm");
    test_matrix.test_combinations_count = 0;
    functionality_test_matrix(&test_matrix, &test_case, true);

    ESP_LOGI(TAG_LV_FILL_FUNC, "running test for ARGB8888 color format:ansi");
    test_matrix.test_combinations_count = 0;
    functionality_test_matrix(&test_matrix, &test_case, false);
}

TEST_CASE("Test fill functionality RGB565", "[fill][functionality][RGB565]")
{
    test_matrix_params_t test_matrix = {
        .min_w = 8,             // 8 is the lower limit for the esp32s3 asm implementation, otherwise esp32 is executed
        .min_h = 1,
        .max_w = 16,
        .max_h = 64,
        .min_unalign_byte = 0,
        .max_unalign_byte = 16,
        .unalign_step = 1,
        .dest_stride_step = 1,
        .test_combinations_count = 0,
    };

    test_case_params_t test_case = {
        .blend_api_func = &lv_draw_sw_blend_color_to_rgb565,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .data_type_size = sizeof(uint16_t),
    };

    ESP_LOGI(TAG_LV_FILL_FUNC, "running test for RGB565 color format:asm");
    test_matrix.test_combinations_count = 0;
    functionality_test_matrix(&test_matrix, &test_case, true);

    ESP_LOGI(TAG_LV_FILL_FUNC, "running test for RGB565 color format:ansi");
    test_matrix.test_combinations_count = 0;
    functionality_test_matrix(&test_matrix, &test_case, false);
}

// ------------------------------------------------ Static test functions ----------------------------------------------

static void functionality_test_matrix(test_matrix_params_t *test_matrix, test_case_params_t *test_case, bool use_asm)
{
    // Step destination array width
    for (int dest_w = test_matrix->min_w; dest_w <= test_matrix->max_w; dest_w++) {

        // Step destination array height
        for (int dest_h = test_matrix->min_h; dest_h <= test_matrix->max_h; dest_h++) {

            // Step destination array stride
            for (int dest_stride = dest_w; dest_stride <= dest_w * 2; dest_stride += test_matrix->dest_stride_step) {

                // Step destination array unalignment
                for (int unalign_byte = test_matrix->min_unalign_byte; unalign_byte <= test_matrix->max_unalign_byte; unalign_byte += test_matrix->unalign_step) {

                    // Call functionality test
                    UPDATE_TEST_CASE(test_case, dest_w, dest_h, dest_stride, unalign_byte);
                    lv_fill_functionality(test_case, use_asm);
                    test_matrix->test_combinations_count++;
                }
            }
        }
    }
    ESP_LOGI(TAG_LV_FILL_FUNC, "test combinations: %d", test_matrix->test_combinations_count);
    perfmon_end(0, 1);

}

static void lv_fill_functionality(test_case_params_t *test_case, bool use_asm)
{
    fill_test_bufs(test_case);

    // Init structure for LVGL blend API, to call the Assembly API
    _lv_draw_sw_blend_fill_dsc_t dsc_asm = {
        .dest_buf = test_case->buf.p_asm,
        .dest_w = test_case->dest_w,
        .dest_h = test_case->dest_h,
        .dest_stride = test_case->dest_stride * test_case->data_type_size,  // stride * sizeof()
        .mask_buf = NULL,
        .color = test_color,
        .opa = LV_OPA_MAX,
        .use_asm = true,
    };

    // Init structure for LVGL blend API, to call the ANSI API
    _lv_draw_sw_blend_fill_dsc_t dsc_ansi = dsc_asm;
    dsc_ansi.dest_buf = test_case->buf.p_ansi;
    dsc_ansi.use_asm = false;

    if(0 == perf_counters[0].start){
        perfmon_start(0, "blend", (true == use_asm) ? "asm": "ansi");
    }

    if(true == use_asm){
        test_case->blend_api_func(&dsc_asm);    // Call the LVGL API with Assembly code
    } else {
        test_case->blend_api_func(&dsc_ansi);   // Call the LVGL API with ANSI code
    }

    // Shift array pointers by Canary Bytes amount back
    test_case->buf.p_asm -= CANARY_BYTES * test_case->data_type_size;
    test_case->buf.p_ansi -= CANARY_BYTES * test_case->data_type_size;

    // Evaluate the results
    sprintf(test_msg_buf, "Test case: dest_w = %d, dest_h = %d, dest_stride = %d, unalign_byte = %d\n", test_case->dest_w, test_case->dest_h, test_case->dest_stride, test_case->unalign_byte);

    switch (test_case->color_format) {
    case LV_COLOR_FORMAT_ARGB8888: {
        test_eval_32bit_data(test_case, use_asm);
        break;
    }

    case LV_COLOR_FORMAT_RGB565: {
        test_eval_16bit_data(test_case, use_asm);
        break;
    }

    default:
        TEST_ASSERT_MESSAGE(false, "LV Color format not found");
    }

    free(test_case->buf.p_asm_alloc);
    free(test_case->buf.p_ansi_alloc);

}

static void fill_test_bufs(test_case_params_t *test_case)
{
    const size_t data_type_size = test_case->data_type_size;        // sizeof() of used data type
    const size_t total_buf_len = test_case->total_buf_len;          // Total buffer length, data part of the buffer including the Canary bytes
    const size_t active_buf_len = test_case->active_buf_len;        // Length of buffer
    const unsigned int unalign_byte = test_case->unalign_byte;

    // Allocate destination arrays for Assembly and ANSI LVGL Blend API
    void *mem_asm   = memalign(16, (total_buf_len * data_type_size) + unalign_byte);
    void *mem_ansi  = memalign(16, (total_buf_len * data_type_size) + unalign_byte);
    TEST_ASSERT_NOT_NULL_MESSAGE(mem_asm, "Lack of memory");
    TEST_ASSERT_NOT_NULL_MESSAGE(mem_ansi, "Lack of memory");

    // Save a pointer to the beginning of the allocated memory which will be used to free()
    test_case->buf.p_asm_alloc = mem_asm;
    test_case->buf.p_ansi_alloc = mem_ansi;

    // Apply destination array unalignment
    uint8_t *dest_buf_asm = (uint8_t *)mem_asm + unalign_byte;
    uint8_t *dest_buf_ansi = (uint8_t *)mem_ansi + unalign_byte;

    // Set the whole buffer to 0, including the Canary bytes part
    memset(dest_buf_asm, 0, total_buf_len * data_type_size);
    memset(dest_buf_ansi, 0, total_buf_len * data_type_size);

    // Fill the actual part of the destination buffers with known values,
    // Values must be same, because of the stride
    for (int i = CANARY_BYTES; i < active_buf_len + CANARY_BYTES; i++) {
        dest_buf_asm[i * data_type_size] = (uint8_t)(i % 255);
        dest_buf_ansi[i * data_type_size] = (uint8_t)(i % 255);
    }

    // Shift array pointers by Canary Bytes amount
    dest_buf_asm += CANARY_BYTES * data_type_size;
    dest_buf_ansi += CANARY_BYTES * data_type_size;

    // Save a pointer to the working part of the memory, where the test data are stored
    test_case->buf.p_asm = (void *)dest_buf_asm;
    test_case->buf.p_ansi = (void *)dest_buf_ansi;
}

static void test_eval_32bit_data(test_case_params_t *test_case, bool use_asm)
{
    // Print results 32bit data
#if DBG_PRINT_OUTPUT
    for (uint32_t i = 0; i < test_case->total_buf_len; i++) {
        printf("dest_buf[%"PRIi32"] %s ansi = %8"PRIx32" \t asm = %8"PRIx32" \n", i, ((i < 10) ? (" ") : ("")), ((uint32_t *)test_case->buf.p_ansi)[i], ((uint32_t *)test_case->buf.p_asm)[i]);
    }
    printf("\n");
#endif

    // Canary bytes area must stay 0
    if(false == use_asm){
        TEST_ASSERT_EACH_EQUAL_UINT32_MESSAGE(0, (uint32_t *)test_case->buf.p_ansi, CANARY_BYTES, test_msg_buf);
    } else {
        TEST_ASSERT_EACH_EQUAL_UINT32_MESSAGE(0, (uint32_t *)test_case->buf.p_asm, CANARY_BYTES, test_msg_buf);
    }

    // dest_buf_asm and dest_buf_ansi must be equal
    // TEST_ASSERT_EQUAL_UINT32_ARRAY_MESSAGE((uint32_t *)test_case->buf.p_asm + CANARY_BYTES, (uint32_t *)test_case->buf.p_ansi + CANARY_BYTES, test_case->active_buf_len, test_msg_buf);

    // Canary bytes area must stay 0
    if(false == use_asm){
        TEST_ASSERT_EACH_EQUAL_UINT32_MESSAGE(0, (uint32_t *)test_case->buf.p_ansi + (test_case->total_buf_len - CANARY_BYTES), CANARY_BYTES, test_msg_buf);
    } else {
        TEST_ASSERT_EACH_EQUAL_UINT32_MESSAGE(0, (uint32_t *)test_case->buf.p_asm + (test_case->total_buf_len - CANARY_BYTES), CANARY_BYTES, test_msg_buf);
    }
}

static void test_eval_16bit_data(test_case_params_t *test_case, bool use_asm)
{
    // Print results, 16bit data
#if DBG_PRINT_OUTPUT
    for (uint32_t i = 0; i < test_case->total_buf_len; i++) {
        printf("dest_buf[%"PRIi32"] %s ansi = %8"PRIx16" \t asm = %8"PRIx16" \n", i, ((i < 10) ? (" ") : ("")), ((uint16_t *)test_bufs.buf_ansi)[i], ((uint16_t *)test_bufs.buf_asm)[i]);
    }
    printf("\n");
#endif

    // Canary bytes area must stay 0
    if(false == use_asm){
        TEST_ASSERT_EACH_EQUAL_UINT16_MESSAGE(0, (uint16_t *)test_case->buf.p_ansi, CANARY_BYTES, test_msg_buf);
    } else {
        TEST_ASSERT_EACH_EQUAL_UINT16_MESSAGE(0, (uint16_t *)test_case->buf.p_asm, CANARY_BYTES, test_msg_buf);
    }

    // dest_buf_asm and dest_buf_ansi must be equal
    // TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE((uint16_t *)test_case->buf.p_asm + CANARY_BYTES, (uint16_t *)test_case->buf.p_ansi + CANARY_BYTES, test_case->active_buf_len, test_msg_buf);

    // Canary bytes area must stay 0
    if(false == use_asm){
        TEST_ASSERT_EACH_EQUAL_UINT16_MESSAGE(0, (uint16_t *)test_case->buf.p_ansi + (test_case->total_buf_len - CANARY_BYTES), CANARY_BYTES, test_msg_buf);
    } else {
        TEST_ASSERT_EACH_EQUAL_UINT16_MESSAGE(0, (uint16_t *)test_case->buf.p_asm + (test_case->total_buf_len - CANARY_BYTES), CANARY_BYTES, test_msg_buf);
    }
}
