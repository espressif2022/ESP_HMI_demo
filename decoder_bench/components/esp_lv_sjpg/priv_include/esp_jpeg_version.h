/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_JPEG_VERION "1.0.0"

/**
 * @version 1.0.0:
 *    Features:
 *      Encoder:
 *          - Support variety of width and height to encoder
 *          - Support RGB888 RGBA YCbYCr YCbY2YCrY2 GRAY raw data
 *          - Support YUV444 YUV422 YUV420 subsampling
 *          - Support quality(1-100)
 *          - Support 0, 90 180 270 degree clockwise rotation, under src_type = JPEG_PIXEL_FORMAT_YCbYCr,
 *            subsampling = JPEG_SUBSAMPLE_420, width and height are multiply of 16 and
 *            src_type = JPEG_PIXEL_FORMAT_YCbYCr, subsampling = JPEG_SUBSAMPLE_GRAY, width and height are multiply of 8.
 *          - Support mono-task and dual-task
 *          - Support two mode encoder, respectively block encoder and one image encoder.
 *      Decoder:
 *          - Support variety of width and height to decoder
 *          - Support one and three channels decoder
 *          - Support RGB888 RGB565(big endian) RGB565(little endian) CbYCrY raw data output
 *          - Support 0, 90 180 270 degree clockwise rotation, under width and height are multiply of 8.
 *          - Support clipper and scale, under width and height are multiply of 8.
 *          - Support two mode encoder, respectively block encoder and one image decoder.
 *          - Support normal and faster decoder method
 *
 * @note :
 *     - The encoder/decoder do ASM optimization in ESP32S3. Frame rate performs better than the others chips.
 */

/**
 * @brief      Get JPEG version string
 *
 * @return
 *       - JPEG codec version
 */
const char *esp_jpeg_get_version();

#ifdef __cplusplus
}
#endif
