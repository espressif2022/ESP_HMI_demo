/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file lv_jpg.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_lv_sjpg.h"
#include "esp_jpeg_dec.h"

#include "lvgl.h"

static char *TAG = "JPG";

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    uint8_t *raw_sjpg_data;             //Used when type==SJPG_IO_SOURCE_C_ARRAY.
    uint32_t raw_sjpg_data_size;        //Num bytes pointed to by raw_sjpg_data.
    lv_fs_file_t lv_file;
} io_source_t;

typedef struct {
    uint8_t *sjpg_data;
    uint32_t sjpg_data_size;
    int sjpg_x_res;
    int sjpg_y_res;
    int sjpg_total_frames;
    int sjpg_single_frame_height;
    int sjpg_cache_frame_index;
    uint8_t **frame_base_array;        //to save base address of each split frames upto sjpg_total_frames.
    int * frame_base_offset;
    uint8_t *frame_cache;
    io_source_t io;
} SJPEG;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header);
static lv_res_t decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc);
static lv_res_t decoder_read_line(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t len, uint8_t *buf);
static void decoder_close(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc);
static int is_jpg(const uint8_t *raw_data, size_t len);
static void lv_sjpg_cleanup(SJPEG *jpg);
static void lv_sjpg_free(SJPEG *jpg);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

esp_err_t esp_lv_split_jpg_init(esp_lv_sjpg_decoder_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    lv_img_decoder_t *dec = lv_img_decoder_create();
    lv_img_decoder_set_info_cb(dec, decoder_info);
    lv_img_decoder_set_open_cb(dec, decoder_open);
    lv_img_decoder_set_close_cb(dec, decoder_close);
    lv_img_decoder_set_read_line_cb(dec, decoder_read_line);

    *ret_handle = dec;
    ESP_LOGD(TAG, "new sjpg decoder @%p", dec);

    ESP_LOGI(TAG, "sjpg decoder create success, version: %d.%d.%d", ESP_LV_SJPG_VER_MAJOR, ESP_LV_SJPG_VER_MINOR, ESP_LV_SJPG_VER_PATCH);
    return ESP_OK;
}

esp_err_t esp_lv_split_jpg_deinit(esp_lv_sjpg_decoder_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid decoder handle pointer");
    ESP_LOGD(TAG, "delete sjpg decoder @%p", handle);
    lv_img_decoder_delete((lv_img_decoder_t *)handle);

    return ESP_OK;
}

static lv_fs_res_t jpg_load_file(const char *filename, uint8_t **buffer, size_t *size, bool read_head)
{
    uint32_t len;
    lv_fs_file_t f;
    lv_fs_res_t res = lv_fs_open(&f, filename, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "Failed to open file %s", filename);
        return res;
    }

    lv_fs_seek(&f, 0, LV_FS_SEEK_END);
    lv_fs_tell(&f, &len);
    lv_fs_seek(&f, 0, LV_FS_SEEK_SET);

    if (read_head && len > 1024) {
        len = 1024;
    } else if (len <= 0) {
        lv_fs_close(&f);
        return LV_FS_RES_FS_ERR;
    }

    *buffer = malloc(len);
    if (!*buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for file %s", filename);
        lv_fs_close(&f);
        return LV_FS_RES_OUT_OF_MEM;
    }

    uint32_t rn = 0;
    res = lv_fs_read(&f, *buffer, len, &rn);
    lv_fs_close(&f);

    if (res != LV_FS_RES_OK || rn != len) {
        free(*buffer);
        *buffer = NULL;
        ESP_LOGE(TAG, "Failed to read file %s", filename);
        return LV_FS_RES_UNKNOWN;
    }
    *size = len;

    return LV_FS_RES_OK;
}

static lv_res_t decode_jpeg(const uint8_t *input_buffer, uint32_t input_size, lv_img_header_t *header, uint8_t **output_buffer)
{
    jpeg_error_t ret;
    jpeg_dec_config_t config = {
#if  LV_COLOR_DEPTH == 32
        .output_type = JPEG_PIXEL_FORMAT_RGB888,
#elif  LV_COLOR_DEPTH == 16
#if  LV_BIG_ENDIAN_SYSTEM == 1 || LV_COLOR_16_SWAP == 1
        .output_type = JPEG_PIXEL_FORMAT_RGB565_BE,
#else
        .output_type = JPEG_PIXEL_FORMAT_RGB565_LE,
#endif
#else
#error Unsupported LV_COLOR_DEPTH
#endif
        .rotate = JPEG_ROTATE_0D,
    };

    jpeg_dec_handle_t jpeg_dec;
    jpeg_dec_open(&config, &jpeg_dec);
    if (!jpeg_dec) {
        ESP_LOGE(TAG, "Failed to open jpeg decoder");
        return LV_RES_INV;
    }

    jpeg_dec_io_t *jpeg_io = malloc(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *out_info = malloc(sizeof(jpeg_dec_header_info_t));
    if (!jpeg_io || !out_info) {
        if (jpeg_io) {
            free(jpeg_io);
        }
        if (out_info) {
            free(out_info);
        }
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to allocate memory for jpeg decoder");
        return LV_RES_INV;
    }

    jpeg_io->inbuf = (unsigned char *)input_buffer;
    jpeg_io->inbuf_len = input_size;

    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret == JPEG_ERR_OK) {

        header->w = out_info->width;
        header->h = out_info->height;
        if (output_buffer) {
            *output_buffer = (uint8_t *)heap_caps_aligned_alloc(16, out_info->height * out_info->width * 2, MALLOC_CAP_DEFAULT);
            if (!*output_buffer) {
                free(jpeg_io);
                free(out_info);
                jpeg_dec_close(jpeg_dec);
                ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                return LV_RES_INV;
            }

            jpeg_io->outbuf = *output_buffer;
            ret = jpeg_dec_process(jpeg_dec, jpeg_io);
            if (ret != JPEG_ERR_OK) {
                free(*output_buffer);
                *output_buffer = NULL;
                free(jpeg_io);
                free(out_info);
                jpeg_dec_close(jpeg_dec);
                ESP_LOGE(TAG, "Failed to decode jpeg:[%d]", ret);
                return LV_RES_INV;
            }
        }
    } else {
        free(jpeg_io);
        free(out_info);
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to parse jpeg header");
        return LV_RES_INV;
    }

    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);

    return LV_RES_OK;
}

static lv_res_t decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{

    LV_UNUSED(decoder);
    lv_img_src_t src_type = lv_img_src_get_type(src);
    lv_res_t lv_ret = LV_RES_OK;

    if (src_type == LV_IMG_SRC_VARIABLE) {

        const lv_img_dsc_t * img_dsc = src;
        uint8_t *raw_sjpeg_data = (uint8_t *)img_dsc->data;

        if (!strncmp((char *)raw_sjpeg_data, "_SJPG__", strlen("_SJPG__"))) {

            raw_sjpeg_data += 14; //seek to res info ... refer sjpg format
            header->always_zero = 0;
            header->cf = LV_IMG_CF_RAW;

            header->w = *raw_sjpeg_data++;
            header->w |= *raw_sjpeg_data++ << 8;

            header->h = *raw_sjpeg_data++;
            header->h |= *raw_sjpeg_data++ << 8;

            return lv_ret;
        } else if (is_jpg(img_dsc->data, img_dsc->data_size) == true) {
            header->always_zero = 0;
            header->cf = LV_IMG_CF_RAW;
            lv_ret = decode_jpeg(img_dsc->data, img_dsc->data_size, header, NULL);

            return lv_ret;
        } else {
            return LV_RES_INV;
        }
    } else if (src_type == LV_IMG_SRC_FILE) {
        const char *fn = src;
        if (strcmp(lv_fs_get_ext(fn), "jpg") == 0) {

            uint8_t *jpg_data = NULL;
            size_t jpg_data_size = 0;

            if (jpg_load_file(fn, &jpg_data, &jpg_data_size, true) != LV_FS_RES_OK) {
                if (jpg_data) {
                    free(jpg_data);
                }
                return LV_RES_INV;
            }

            lv_ret = decode_jpeg(jpg_data, jpg_data_size, header, NULL);
            free(jpg_data);

            return lv_ret;
        } else if (strcmp(lv_fs_get_ext(fn), "sjpg") == 0) {

            uint8_t buff[22];
            memset(buff, 0, sizeof(buff));

            lv_fs_file_t file;
            lv_fs_res_t res = lv_fs_open(&file, fn, LV_FS_MODE_RD);
            if (res != LV_FS_RES_OK) {
                return 78;
            }

            uint32_t rn;
            res = lv_fs_read(&file, buff, 8, &rn);
            if (res != LV_FS_RES_OK || rn != 8) {
                lv_fs_close(&file);
                return LV_RES_INV;
            }

            if (strcmp((char *)buff, "_SJPG__") == 0) {
                lv_fs_seek(&file, 14, LV_FS_SEEK_SET);
                res = lv_fs_read(&file, buff, 4, &rn);
                if (res != LV_FS_RES_OK || rn != 4) {
                    lv_fs_close(&file);
                    return LV_RES_INV;
                }
                header->always_zero = 0;
                header->cf = LV_IMG_CF_TRUE_COLOR;
                uint8_t * raw_jpg_data = buff;
                header->w = *raw_jpg_data++;
                header->w |= *raw_jpg_data++ << 8;
                header->h = *raw_jpg_data++;
                header->h |= *raw_jpg_data++ << 8;
                lv_fs_close(&file);
                return LV_RES_OK;
            }
        } else {
            return LV_RES_INV;
        }
    }

    return LV_RES_INV;
}

static lv_res_t decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    LV_UNUSED(decoder);
    lv_res_t lv_ret = LV_RES_OK;

    if (dsc->src_type == LV_IMG_SRC_VARIABLE) {

        uint8_t *data;
        SJPEG *sjpg = (SJPEG *) dsc->user_data;
        const uint32_t raw_sjpg_data_size = ((lv_img_dsc_t *)dsc->src)->data_size;
        if (sjpg == NULL) {
            sjpg =  malloc(sizeof(SJPEG));
            if (!sjpg) {
                return LV_RES_INV;
            }

            memset(sjpg, 0, sizeof(SJPEG));

            dsc->user_data = sjpg;
            sjpg->sjpg_data = (uint8_t *)((lv_img_dsc_t *)(dsc->src))->data;
            sjpg->sjpg_data_size = ((lv_img_dsc_t *)(dsc->src))->data_size;
        }

        if (!strncmp((char *) sjpg->sjpg_data, "_SJPG__", strlen("_SJPG__"))) {
            data = sjpg->sjpg_data;
            data += 14;

            sjpg->sjpg_x_res = *data++;
            sjpg->sjpg_x_res |= *data++ << 8;

            sjpg->sjpg_y_res = *data++;
            sjpg->sjpg_y_res |= *data++ << 8;

            sjpg->sjpg_total_frames = *data++;
            sjpg->sjpg_total_frames |= *data++ << 8;

            sjpg->sjpg_single_frame_height = *data++;
            sjpg->sjpg_single_frame_height |= *data++ << 8;

            ESP_LOGD(TAG, "[%d,%d], frames:%d, height:%d", sjpg->sjpg_x_res, sjpg->sjpg_y_res, \
                     sjpg->sjpg_total_frames, sjpg->sjpg_single_frame_height);

            sjpg->frame_base_array = malloc(sizeof(uint8_t *) * sjpg->sjpg_total_frames);
            if (! sjpg->frame_base_array) {
                ESP_LOGE(TAG, "Not enough memory for frame_base_array allocation");
                lv_sjpg_cleanup(sjpg);
                sjpg = NULL;
                return LV_RES_INV;
            }

            uint8_t *img_frame_base = data +  sjpg->sjpg_total_frames * 2;
            sjpg->frame_base_array[0] = img_frame_base;

            for (int i = 1; i <  sjpg->sjpg_total_frames; i++) {
                int offset = *data++;
                offset |= *data++ << 8;
                sjpg->frame_base_array[i] = sjpg->frame_base_array[i - 1] + offset;
            }
            sjpg->sjpg_cache_frame_index = -1;
            sjpg->frame_cache = (void *)malloc(sjpg->sjpg_x_res * sjpg->sjpg_single_frame_height * 4);
            if (! sjpg->frame_cache) {
                ESP_LOGE(TAG, "Not enough memory for frame_cache allocation");
                lv_sjpg_cleanup(sjpg);
                sjpg = NULL;
                return LV_RES_INV;
            }
            dsc->img_data = NULL;

            return lv_ret;
        } else if (is_jpg(sjpg->sjpg_data, raw_sjpg_data_size) == true) {
            uint8_t *output_buffer = NULL;
            lv_img_header_t header;

            lv_ret = decode_jpeg(sjpg->sjpg_data, raw_sjpg_data_size, &header, &output_buffer);
            if (lv_ret == LV_RES_OK) {
                dsc->img_data = output_buffer;
                sjpg->frame_cache = output_buffer;
            } else {
                if (output_buffer) {
                    free(output_buffer);
                }
                ESP_LOGE(TAG, "Decode (esp_jpg) error");
                lv_sjpg_cleanup(sjpg);
                sjpg = NULL;
            }

            return lv_ret;
        } else {
            return LV_RES_INV;
        }
    } else if (dsc->src_type == LV_IMG_SRC_FILE) {
        const char *fn = dsc->src;
        if (strcmp(lv_fs_get_ext(fn), "jpg") == 0) {
            uint8_t *jpg_data = NULL;
            size_t jpg_data_size = 0;

            SJPEG *jpg = (SJPEG *) dsc->user_data;
            if (jpg == NULL) {
                jpg = malloc(sizeof(SJPEG));
                if (!jpg) {
                    ESP_LOGE(TAG, "Failed to allocate memory for jpg");
                    return LV_RES_INV;
                }

                memset(jpg, 0, sizeof(SJPEG));
                dsc->user_data = jpg;
            }

            if (jpg_load_file(fn, &jpg_data, &jpg_data_size, false) != LV_FS_RES_OK) {
                if (jpg_data) {
                    free(jpg_data);
                    lv_sjpg_cleanup(jpg);
                }
                return LV_RES_INV;
            }

            uint8_t *output_buffer = NULL;
            lv_img_header_t header;

            lv_ret = decode_jpeg(jpg_data, jpg_data_size, &header, &output_buffer);
            free(jpg_data);

            if (lv_ret == LV_RES_OK) {
                dsc->img_data = output_buffer;
                jpg->frame_cache = output_buffer;
            } else {
                ESP_LOGE(TAG, "Decode (esp_jpg) error");
                if (output_buffer) {
                    free(output_buffer);
                }
                lv_sjpg_cleanup(jpg);
                jpg = NULL;
            }

            return lv_ret;
        } else if (strcmp(lv_fs_get_ext(fn), "sjpg") == 0) {
            uint8_t *data;
            uint8_t buff[22];
            memset(buff, 0, sizeof(buff));

            lv_fs_file_t lv_file;
            lv_fs_res_t res = lv_fs_open(&lv_file, fn, LV_FS_MODE_RD);
            if (res != LV_FS_RES_OK) {
                return 78;
            }

            uint32_t rn;
            res = lv_fs_read(&lv_file, buff, 22, &rn);
            if (res != LV_FS_RES_OK || rn != 22) {
                lv_fs_close(&lv_file);
                return LV_RES_INV;
            }

            if (strcmp((char *)buff, "_SJPG__") == 0) {

                SJPEG *sjpg = (SJPEG *)dsc->user_data;
                if (sjpg == NULL) {
                    sjpg = malloc(sizeof(SJPEG));
                    if (! sjpg) {
                        ESP_LOGI(TAG, "Failed to allocate memory for sjpg");
                        lv_fs_close(&lv_file);
                        return LV_RES_INV;
                    }
                    memset(sjpg, 0, sizeof(SJPEG));
                    dsc->user_data = sjpg;
                }
                data = buff;
                data += 14;

                sjpg->sjpg_x_res = *data++;
                sjpg->sjpg_x_res |= *data++ << 8;

                sjpg->sjpg_y_res = *data++;
                sjpg->sjpg_y_res |= *data++ << 8;

                sjpg->sjpg_total_frames = *data++;
                sjpg->sjpg_total_frames |= *data++ << 8;

                sjpg->sjpg_single_frame_height = *data++;
                sjpg->sjpg_single_frame_height |= *data++ << 8;

                ESP_LOGD(TAG, "[%d,%d], frames:%d, height:%d", sjpg->sjpg_x_res, sjpg->sjpg_y_res, \
                         sjpg->sjpg_total_frames, sjpg->sjpg_single_frame_height);
                sjpg->frame_base_offset = malloc(sizeof(int *) * sjpg->sjpg_total_frames);
                if (! sjpg->frame_base_offset) {
                    ESP_LOGE(TAG, "Not enough memory for frame_base_offset allocation");
                    lv_sjpg_cleanup(sjpg);
                    sjpg = NULL;
                    return LV_RES_INV;
                }

                int img_frame_start_offset = 22 +  sjpg->sjpg_total_frames * 2;
                sjpg->frame_base_offset[0] = img_frame_start_offset;

                for (int i = 1; i <  sjpg->sjpg_total_frames; i++) {
                    res = lv_fs_read(&lv_file, buff, 2, &rn);
                    if (res != LV_FS_RES_OK || rn != 2) {
                        lv_fs_close(&lv_file);
                        return LV_RES_INV;
                    }

                    data = buff;
                    int offset = *data++;
                    offset |= *data++ << 8;
                    sjpg->frame_base_offset[i] = sjpg->frame_base_offset[i - 1] + offset;
                }

                sjpg->sjpg_cache_frame_index = -1;
                sjpg->frame_cache = (void *)malloc(sjpg->sjpg_x_res * sjpg->sjpg_single_frame_height * 4);
                if (!sjpg->frame_cache) {
                    ESP_LOGE(TAG, "Not enough memory for frame_cache allocation");
                    lv_fs_close(&lv_file);
                    lv_sjpg_cleanup(sjpg);
                    sjpg = NULL;
                    return LV_RES_INV;
                }
                uint32_t total_size;
                lv_fs_seek(&lv_file, 0, LV_FS_SEEK_END);
                lv_fs_tell(&lv_file, &total_size);
                sjpg->sjpg_data_size = total_size;

                sjpg->io.lv_file = lv_file;
                dsc->img_data = NULL;
                return lv_ret;
            }
        } else {
            return LV_RES_INV;
        }
    }

    return LV_RES_INV;
}

/**
 * Decode `len` pixels starting from the given `x`, `y` coordinates and store them in `buf`.
 * Required only if the "open" function can't open the whole decoded pixel array. (dsc->img_data == NULL)
 * @param decoder pointer to the decoder the function associated with
 * @param dsc pointer to decoder descriptor
 * @param x start x coordinate
 * @param y start y coordinate
 * @param len number of pixels to decode
 * @param buf a buffer to store the decoded pixels
 * @return LV_RES_OK: ok; LV_RES_INV: failed
 */

static lv_res_t decoder_read_line(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t len, uint8_t *buf)
{
    LV_UNUSED(decoder);

    lv_res_t error;
    uint8_t *img_data = NULL;

    uint8_t color_depth = 0;

#if LV_COLOR_DEPTH == 32
    color_depth = 4;
#elif LV_COLOR_DEPTH == 16
    color_depth = 2;
#else
#error Unsupported LV_COLOR_DEPTH
#endif

    if (dsc->src_type == LV_IMG_SRC_FILE) {
        uint32_t rn = 0;
        lv_fs_res_t res;

        SJPEG *sjpg = (SJPEG *) dsc->user_data;

        lv_fs_file_t *lv_file_p = &(sjpg->io.lv_file);
        if (!lv_file_p) {
            ESP_LOGI(TAG, "lv_img_decoder_read_line: lv_file_p");
            return LV_RES_INV;
        }
        int jpg_req_frame_index = y / sjpg->sjpg_single_frame_height;
        /*If line not from cache, refresh cache */
        if (jpg_req_frame_index != sjpg->sjpg_cache_frame_index) {

            if (jpg_req_frame_index == (sjpg->sjpg_total_frames - 1)) {
                /*This is the last frame. */
                sjpg->io.raw_sjpg_data_size = sjpg->sjpg_data_size - (uint32_t)(sjpg->frame_base_offset[jpg_req_frame_index]);
            } else {
                sjpg->io.raw_sjpg_data_size =
                    (uint32_t)(sjpg->frame_base_offset[jpg_req_frame_index + 1] - (uint32_t)(sjpg->frame_base_offset[jpg_req_frame_index]));
            }

            int next_read_pos = (int)(sjpg->frame_base_offset [ jpg_req_frame_index ]);
            lv_fs_seek(lv_file_p, next_read_pos, LV_FS_SEEK_SET);
            res = lv_fs_read(lv_file_p, sjpg->frame_cache, sjpg->io.raw_sjpg_data_size, &rn);

            if (res != LV_FS_RES_OK || rn != sjpg->io.raw_sjpg_data_size) {
                lv_fs_close(lv_file_p);
                return LV_RES_INV;
            }

            lv_img_header_t header;
            error = decode_jpeg(sjpg->frame_cache, rn, &header, &img_data);
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (esp_jpg) error");
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                memcpy(sjpg->frame_cache, img_data, sjpg->sjpg_x_res * sjpg->sjpg_single_frame_height * color_depth);
                if (img_data != NULL) {
                    free(img_data);
                }
            }
            sjpg->sjpg_cache_frame_index = jpg_req_frame_index;
        }

        uint8_t *cache = (uint8_t *)sjpg->frame_cache + x * color_depth + (y % sjpg->sjpg_single_frame_height) * sjpg->sjpg_x_res * color_depth;
        memcpy(buf, cache, color_depth * len);
        return LV_RES_OK;
    } else if (dsc->src_type == LV_IMG_SRC_VARIABLE) {
        SJPEG *sjpg = (SJPEG *) dsc->user_data;

        int sjpg_req_frame_index = y / sjpg->sjpg_single_frame_height;

        /*If line not from cache, refresh cache */
        if (sjpg_req_frame_index != sjpg->sjpg_cache_frame_index) {
            sjpg->io.raw_sjpg_data = sjpg->frame_base_array[ sjpg_req_frame_index ];
            if (sjpg_req_frame_index == (sjpg->sjpg_total_frames - 1)) {
                /*This is the last frame. */
                const uint32_t frame_offset = (uint32_t)(sjpg->io.raw_sjpg_data - sjpg->sjpg_data);
                sjpg->io.raw_sjpg_data_size = sjpg->sjpg_data_size - frame_offset;
            } else {
                sjpg->io.raw_sjpg_data_size =
                    (uint32_t)(sjpg->frame_base_array[sjpg_req_frame_index + 1] - sjpg->io.raw_sjpg_data);
            }

            lv_img_header_t header;             /*No used, just required by the decoder*/
            error = decode_jpeg(sjpg->io.raw_sjpg_data, sjpg->io.raw_sjpg_data_size, &header, &img_data);
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (esp_jpg) error");
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                memcpy(sjpg->frame_cache, img_data, sjpg->sjpg_single_frame_height * sjpg->sjpg_x_res * color_depth);
                if (img_data != NULL) {
                    free(img_data);
                }
            }
            sjpg->sjpg_cache_frame_index = sjpg_req_frame_index;
        }
        uint8_t *cache = (uint8_t *)sjpg->frame_cache + x * color_depth + (y % sjpg->sjpg_single_frame_height) * sjpg->sjpg_x_res * color_depth;
        memcpy(buf, cache, color_depth * len);
        return LV_RES_OK;
    }

    return LV_RES_INV;
}

/**
 * Free the allocated resources
 * @param decoder pointer to the decoder where this function belongs
 * @param dsc pointer to a descriptor which describes this decoding session
 */
static void decoder_close(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    LV_UNUSED(decoder);
    /*Free all allocated data*/
    SJPEG *jpg = (SJPEG *) dsc->user_data;
    if (!jpg) {
        return;
    }

    switch (dsc->src_type) {
    case LV_IMG_SRC_FILE:
        if (jpg->io.lv_file.file_d) {
            lv_fs_close(&(jpg->io.lv_file));
        }
        lv_sjpg_cleanup(jpg);
        break;

    case LV_IMG_SRC_VARIABLE:
        lv_sjpg_cleanup(jpg);
        break;

    default:
        ;
    }
}

static int is_jpg(const uint8_t *raw_data, size_t len)
{
    const uint8_t jpg_signature_JFIF[] = {0xFF, 0xD8, 0xFF,  0xE0,  0x00,  0x10, 0x4A,  0x46, 0x49, 0x46};
    const uint8_t jpg_signature_Adobe[] = {0xFF, 0xD8, 0xFF,  0xEE,  0x00, 0x0E,  0x41, 0x64, 0x6F, 0x62};
    if (len < sizeof(jpg_signature_JFIF)) {
        return false;
    }
    return ((memcmp(jpg_signature_JFIF, raw_data, sizeof(jpg_signature_JFIF)) == 0) | (memcmp(jpg_signature_Adobe, raw_data, sizeof(jpg_signature_Adobe)) == 0));
}

static void lv_sjpg_free(SJPEG *jpg)
{
    if (jpg->frame_cache) {
        free(jpg->frame_cache);
    }
    if (jpg->frame_base_array) {
        free(jpg->frame_base_array);
    }
    if (jpg->frame_base_offset) {
        free(jpg->frame_base_offset);
    }
}

static void lv_sjpg_cleanup(SJPEG *jpg)
{
    if (!jpg) {
        return;
    }

    lv_sjpg_free(jpg);
    free(jpg);
}
