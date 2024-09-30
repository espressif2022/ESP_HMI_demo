/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_lv_sqoi.h"

#include "lvgl.h"

#define QOI_IMPLEMENTATION
#include "qoi.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    uint8_t *raw_qoi_data;             //Used when type==QOI_IO_SOURCE_C_ARRAY.
    uint32_t raw_qoi_data_size;        //Num bytes pointed to by raw_qoi_data.
    lv_fs_file_t lv_file;
} io_source_t;

typedef struct {
    uint8_t *qoi_data;
    uint32_t qoi_data_size;
    int qoi_x_res;
    int qoi_y_res;
    int qoi_total_frames;
    int qoi_single_frame_height;
    int qoi_cache_frame_index;
    uint8_t **frame_base_array;         //to save base address of each split frames upto qoi_total_frames.
    int * frame_base_offset;            //to save base offset for fseek
    uint8_t *frame_cache;
    io_source_t io;
} SQOI;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t decoder_info(struct _lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header);
static lv_res_t decoder_open(lv_img_decoder_t *dec, lv_img_decoder_dsc_t *dsc);
static lv_res_t decoder_read_line(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t len, uint8_t *buf);
static void decoder_close(lv_img_decoder_t *dec, lv_img_decoder_dsc_t *dsc);
static void convert_color_depth(uint8_t *img, uint32_t px_cnt);
static int is_qoi(const uint8_t *raw_data, size_t len);
static void lv_qoi_cleanup(SQOI *qoi);
static void lv_qoi_free(SQOI *qoi);

static lv_res_t qoi_decode32(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, size_t insize);

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "sqoi";

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Register the PNG decoder functions in LVGL
 */
esp_err_t esp_lv_split_qoi_init(esp_lv_sqoi_decoder_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    lv_img_decoder_t *dec = lv_img_decoder_create();
    lv_img_decoder_set_info_cb(dec, decoder_info);
    lv_img_decoder_set_open_cb(dec, decoder_open);
    lv_img_decoder_set_close_cb(dec, decoder_close);
    lv_img_decoder_set_read_line_cb(dec, decoder_read_line);

    *ret_handle = dec;
    ESP_LOGD(TAG, "new qoi decoder @%p", dec);

    ESP_LOGD(TAG, "qoi decoder create success, version: %d.%d.%d", ESP_LV_SQOI_VER_MAJOR, ESP_LV_SQOI_VER_MINOR, ESP_LV_SQOI_VER_PATCH);
    return ESP_OK;
}

esp_err_t esp_lv_split_qoi_deinit(esp_lv_sqoi_decoder_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid decoder handle pointer");
    ESP_LOGD(TAG, "delete qoi decoder @%p", handle);
    lv_img_decoder_delete(handle);

    return ESP_OK;

}

/**********************
 *   STATIC FUNCTIONS
 **********************/

lv_res_t qoi_decode32(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, size_t insize)
{
    if (!in || !out || !w || !h) {
        return LV_RES_INV;
    }

    qoi_desc image;
    memset(&image, 0, sizeof(image));

    unsigned char *pixels = qoi_decode(in, insize, &image, 0);

    *w = image.width;
    *h = image.height;
    *out = pixels;
    if (*out == NULL) {
        return LV_RES_INV;
    }
    return LV_RES_OK;
}

static lv_fs_res_t qoi_load_file(const char *filename, uint8_t **buffer, size_t *size, bool read_head)
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
        len = 100;
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

/**
 * Get info about a PNG image
 * @param src can be file name or pointer to a C array
 * @param header store the info here
 * @return LV_RES_OK: no error; LV_RES_INV: can't get the info
 */
static lv_res_t decoder_info(struct _lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    (void) decoder; /*Unused*/
    lv_img_src_t src_type = lv_img_src_get_type(src);          /*Get the source type*/

    lv_res_t lv_ret = LV_RES_OK;

    if (src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t *img_dsc = src;
        uint8_t *raw_qoi_data = (uint8_t *)img_dsc->data;
        const uint32_t data_size = img_dsc->data_size;
        const uint8_t *size = ((uint8_t *)img_dsc->data) + 4;

        if (!strncmp((char *)raw_qoi_data, "_SQOI__", strlen("_SQOI__"))) {
            raw_qoi_data += 14; //seek to res info ... refer qoi format
            header->always_zero = 0;
            header->cf = LV_IMG_CF_RAW_ALPHA;

            header->w = *raw_qoi_data++;
            header->w |= *raw_qoi_data++ << 8;

            header->h = *raw_qoi_data++;
            header->h |= *raw_qoi_data++ << 8;

            return lv_ret;
        } else if (is_qoi(raw_qoi_data, data_size) == true) {
            header->always_zero = 0;

            if (img_dsc->header.cf) {
                header->cf = img_dsc->header.cf;       /*Save the color format*/
            } else {
                header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            }

            if (img_dsc->header.w) {
                header->w = img_dsc->header.w;         /*Save the image width*/
            } else {
                header->w = (lv_coord_t)((size[0] << 24) + (size[1] << 16) + (size[2] << 8) + (size[3] << 0));
            }

            if (img_dsc->header.h) {
                header->h = img_dsc->header.h;         /*Save the color height*/
            } else {
                header->h = (lv_coord_t)((size[4] << 24) + (size[5] << 16) + (size[6] << 8) + (size[7] << 0));
            }

            return lv_ret;
        } else {
            return LV_RES_INV;
        }
    } else if (src_type == LV_IMG_SRC_FILE) {
        const char *fn = src;
        if (strcmp(lv_fs_get_ext(fn), "qoi") == 0) {
            uint8_t *qoi_data = NULL;   /*Pointer to the loaded data. Same as the original file just loaded into the RAM*/
            size_t qoi_data_size;       /*Size of `qoi_data` in bytes*/

            if (qoi_load_file(fn, &qoi_data, &qoi_data_size, true) != LV_FS_RES_OK) {
                if (qoi_data) {
                    free(qoi_data);
                }
                return LV_RES_INV;
            }

            const uint8_t *size = ((uint8_t *)qoi_data) + 4;
            header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            header->w = (lv_coord_t)((size[0] << 24) + (size[1] << 16) + (size[2] << 8) + (size[3] << 0));
            header->h = (lv_coord_t)((size[4] << 24) + (size[5] << 16) + (size[6] << 8) + (size[7] << 0));
            free(qoi_data);

            return lv_ret;
        } else if (strcmp(lv_fs_get_ext(fn), "sqoi") == 0) {

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

            if (strcmp((char *)buff, "_SQOI__") == 0) {
                lv_fs_seek(&file, 14, LV_FS_SEEK_SET);
                res = lv_fs_read(&file, buff, 4, &rn);
                if (res != LV_FS_RES_OK || rn != 4) {
                    lv_fs_close(&file);
                    return LV_RES_INV;
                }
                header->always_zero = 0;
                header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
                uint8_t * raw_qoi_data = buff;
                header->w = *raw_qoi_data++;
                header->w |= *raw_qoi_data++ << 8;
                header->h = *raw_qoi_data++;
                header->h |= *raw_qoi_data++ << 8;
                lv_fs_close(&file);
                return LV_RES_OK;
            }
        } else {
            return LV_RES_INV;
        }
    }

    return LV_RES_INV;         /*If didn't succeeded earlier then it's an error*/
}

/**
 * Open a PNG image and return the decided image
 * @param src can be file name or pointer to a C array
 * @param style style of the image object (unused now but certain formats might use it)
 * @return pointer to the decoded image or `LV_IMG_DECODER_OPEN_FAIL` if failed
 */
static lv_res_t decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{

    (void) decoder; /*Unused*/
    lv_res_t lv_ret = LV_RES_OK;        /*For the return values of PNG decoder functions*/

    uint8_t *img_data = NULL;

    if (dsc->src_type == LV_IMG_SRC_VARIABLE) {

        const lv_img_dsc_t *img_dsc = dsc->src;
        uint32_t png_width;             /*No used, just required by he decoder*/
        uint32_t png_height;            /*No used, just required by he decoder*/

        uint8_t *data;
        SQOI *qoi = (SQOI *) dsc->user_data;
        const uint32_t raw_qoi_data_size = ((lv_img_dsc_t *)dsc->src)->data_size;
        if (qoi == NULL) {
            qoi =  malloc(sizeof(SQOI));
            if (!qoi) {
                return LV_RES_INV;
            }

            memset(qoi, 0, sizeof(SQOI));

            dsc->user_data = qoi;
            qoi->qoi_data = (uint8_t *)((lv_img_dsc_t *)(dsc->src))->data;
            qoi->qoi_data_size = ((lv_img_dsc_t *)(dsc->src))->data_size;
        }

        if (!strncmp((char *) qoi->qoi_data, "_SQOI__", strlen("_SQOI__"))) {
            data = qoi->qoi_data;
            data += 14;

            qoi->qoi_x_res = *data++;
            qoi->qoi_x_res |= *data++ << 8;

            qoi->qoi_y_res = *data++;
            qoi->qoi_y_res |= *data++ << 8;

            qoi->qoi_total_frames = *data++;
            qoi->qoi_total_frames |= *data++ << 8;

            qoi->qoi_single_frame_height = *data++;
            qoi->qoi_single_frame_height |= *data++ << 8;

            ESP_LOGD(TAG, "[%d,%d], frames:%d, height:%d", qoi->qoi_x_res, qoi->qoi_y_res, \
                     qoi->qoi_total_frames, qoi->qoi_single_frame_height);
            qoi->frame_base_array = malloc(sizeof(uint8_t *) * qoi->qoi_total_frames);
            if (! qoi->frame_base_array) {
                ESP_LOGE(TAG, "Not enough memory for frame_base_array allocation");
                lv_qoi_cleanup(qoi);
                qoi = NULL;
                return LV_RES_INV;
            }

            uint8_t *img_frame_base = data +  qoi->qoi_total_frames * 2;
            qoi->frame_base_array[0] = img_frame_base;

            for (int i = 1; i <  qoi->qoi_total_frames; i++) {
                int offset = *data++;
                offset |= *data++ << 8;
                qoi->frame_base_array[i] = qoi->frame_base_array[i - 1] + offset;
            }
            qoi->qoi_cache_frame_index = -1;
            qoi->frame_cache = (void *)malloc(qoi->qoi_x_res * qoi->qoi_single_frame_height * 4);
            if (! qoi->frame_cache) {
                ESP_LOGE(TAG, "Not enough memory for frame_cache allocation");
                lv_qoi_cleanup(qoi);
                qoi = NULL;
                return LV_RES_INV;
            }
            dsc->img_data = NULL;

            return lv_ret;
        } else if (is_qoi(qoi->qoi_data, raw_qoi_data_size) == true) {
            /*Decode the image in ARGB8888 */
            lv_ret = qoi_decode32(&img_data, &png_width, &png_height, img_dsc->data, img_dsc->data_size);
            if (lv_ret != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (qoi_decode32) error:%d", lv_ret);
                if (img_data != NULL) {
                    free(img_data);
                }
                lv_qoi_cleanup(qoi);
                qoi = NULL;
                return LV_RES_INV;
            } else {
                /*Convert the image to the system's color depth*/
                convert_color_depth(img_data,  png_width * png_height);
                dsc->img_data = img_data;
                qoi->frame_cache = img_data;
            }
            return lv_ret;
        } else {
            return LV_RES_INV;
        }
        return LV_RES_OK;     /*Return with its pointer*/
    } else if (dsc->src_type == LV_IMG_SRC_FILE) {
        const char *fn = dsc->src;
        uint32_t png_width;
        uint32_t png_height;

        if (strcmp(lv_fs_get_ext(fn), "qoi") == 0) {
            uint8_t *qoi_data = NULL;      /*Pointer to the loaded data. Same as the original file just loaded into the RAM*/
            size_t qoi_data_size;   /*Size of `qoi_data` in bytes*/

            SQOI *qoi = (SQOI *) dsc->user_data;
            if (qoi == NULL) {
                qoi = malloc(sizeof(SQOI));
                if (!qoi) {
                    ESP_LOGE(TAG, "Failed to allocate memory for qoi");
                    return LV_RES_INV;
                }

                memset(qoi, 0, sizeof(SQOI));
                dsc->user_data = qoi;
            }

            if (qoi_load_file(fn, &qoi_data, &qoi_data_size, false) != LV_FS_RES_OK) {
                if (qoi_data) {
                    free(qoi_data);
                    lv_qoi_cleanup(qoi);
                }
                return LV_RES_INV;
            }

            lv_ret = qoi_decode32(&img_data, &png_width, &png_height, qoi_data, qoi_data_size);
            free(qoi_data);
            if (lv_ret != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (qoi_decode32) error:%d", lv_ret);
                if (img_data != NULL) {
                    free(img_data);
                }
                lv_qoi_cleanup(qoi);
                qoi = NULL;
                return LV_RES_INV;
            } else {
                /*Convert the image to the system's color depth*/
                convert_color_depth(img_data,  png_width * png_height);
                dsc->img_data = img_data;
                qoi->frame_cache = img_data;
            }
            return lv_ret;
        } else if (strcmp(lv_fs_get_ext(fn), "sqoi") == 0) {
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

            if (strcmp((char *)buff, "_SQOI__") == 0) {

                SQOI *sqoi = (SQOI *)dsc->user_data;
                if (sqoi == NULL) {
                    sqoi = malloc(sizeof(SQOI));
                    if (! sqoi) {
                        ESP_LOGI(TAG, "Failed to allocate memory for sqoi");
                        lv_fs_close(&lv_file);
                        return LV_RES_INV;
                    }
                    memset(sqoi, 0, sizeof(SQOI));
                    dsc->user_data = sqoi;
                }
                data = buff;
                data += 14;

                sqoi->qoi_x_res = *data++;
                sqoi->qoi_x_res |= *data++ << 8;

                sqoi->qoi_y_res = *data++;
                sqoi->qoi_y_res |= *data++ << 8;

                sqoi->qoi_total_frames = *data++;
                sqoi->qoi_total_frames |= *data++ << 8;

                sqoi->qoi_single_frame_height = *data++;
                sqoi->qoi_single_frame_height |= *data++ << 8;

                ESP_LOGD(TAG, "[%d,%d], frames:%d, height:%d", sqoi->qoi_x_res, sqoi->qoi_y_res, \
                         sqoi->qoi_total_frames, sqoi->qoi_single_frame_height);
                sqoi->frame_base_offset = malloc(sizeof(int *) * sqoi->qoi_total_frames);
                if (! sqoi->frame_base_offset) {
                    ESP_LOGE(TAG, "Not enough memory for frame_base_offset allocation");
                    lv_qoi_cleanup(sqoi);
                    sqoi = NULL;
                    return LV_RES_INV;
                }

                int img_frame_start_offset = 22 +  sqoi->qoi_total_frames * 2;
                sqoi->frame_base_offset[0] = img_frame_start_offset;

                for (int i = 1; i <  sqoi->qoi_total_frames; i++) {
                    res = lv_fs_read(&lv_file, buff, 2, &rn);
                    if (res != LV_FS_RES_OK || rn != 2) {
                        lv_fs_close(&lv_file);
                        return LV_RES_INV;
                    }

                    data = buff;
                    int offset = *data++;
                    offset |= *data++ << 8;
                    sqoi->frame_base_offset[i] = sqoi->frame_base_offset[i - 1] + offset;
                }

                sqoi->qoi_cache_frame_index = -1; //INVALID AT BEGINNING for a forced compare mismatch at first time.
                sqoi->frame_cache = (void *)malloc(sqoi->qoi_x_res * sqoi->qoi_single_frame_height * 4);
                if (!sqoi->frame_cache) {
                    ESP_LOGE(TAG, "Not enough memory for frame_cache allocation");
                    lv_fs_close(&lv_file);
                    lv_qoi_cleanup(sqoi);
                    sqoi = NULL;
                    return LV_RES_INV;
                }

                uint32_t total_size;
                lv_fs_seek(&lv_file, 0, LV_FS_SEEK_END);
                lv_fs_tell(&lv_file, &total_size);
                sqoi->qoi_data_size = total_size;

                sqoi->io.lv_file = lv_file;
                dsc->img_data = NULL;
                return lv_ret;
            }
        } else {
            return LV_RES_INV;
        }
    }

    return LV_RES_INV;    /*If not returned earlier then it failed*/
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
    color_depth = 3;
#elif LV_COLOR_DEPTH == 8
    color_depth = 2;
#elif LV_COLOR_DEPTH == 1
    color_depth = 2;
#endif

    if (dsc->src_type == LV_IMG_SRC_FILE) {
        uint32_t rn = 0;
        lv_fs_res_t res;

        SQOI *sqoi = (SQOI *) dsc->user_data;

        lv_fs_file_t *lv_file_p = &(sqoi->io.lv_file);
        if (!lv_file_p) {
            ESP_LOGI(TAG, "lv_img_decoder_read_line: lv_file_p");
            return LV_RES_INV;
        }

        int qoi_req_frame_index = y / sqoi->qoi_single_frame_height;
        /*If line not from cache, refresh cache */
        if (qoi_req_frame_index != sqoi->qoi_cache_frame_index) {

            if (qoi_req_frame_index == (sqoi->qoi_total_frames - 1)) {
                /*This is the last frame. */
                sqoi->io.raw_qoi_data_size = sqoi->qoi_data_size - (uint32_t)(sqoi->frame_base_offset[qoi_req_frame_index]);
            } else {
                sqoi->io.raw_qoi_data_size =
                    (uint32_t)(sqoi->frame_base_offset[qoi_req_frame_index + 1] - (uint32_t)(sqoi->frame_base_offset[qoi_req_frame_index]));
            }

            int next_read_pos = (int)(sqoi->frame_base_offset [ qoi_req_frame_index ]);
            lv_fs_seek(lv_file_p, next_read_pos, LV_FS_SEEK_SET);
            res = lv_fs_read(lv_file_p, sqoi->frame_cache, sqoi->io.raw_qoi_data_size, &rn);
            if (res != LV_FS_RES_OK || rn != sqoi->io.raw_qoi_data_size) {
                lv_fs_close(lv_file_p);
                return LV_RES_INV;
            }

            uint32_t png_width;             /*No used, just required by he decoder*/
            uint32_t png_height;            /*No used, just required by he decoder*/

            /*Decode the image in ARGB8888 */
            error = qoi_decode32(&img_data, &png_width, &png_height, sqoi->frame_cache, rn);
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (qoi_decode32) error:%d", error);
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                convert_color_depth(img_data,  png_width * png_height);
                memcpy(sqoi->frame_cache, img_data, png_width * png_height * color_depth);
                if (img_data != NULL) {
                    free(img_data);
                }
            }
            sqoi->qoi_cache_frame_index = qoi_req_frame_index;
        }

        uint8_t *cache = (uint8_t *)sqoi->frame_cache + x * color_depth + (y % sqoi->qoi_single_frame_height) * sqoi->qoi_x_res * color_depth;
        memcpy(buf, cache, color_depth * len);
        return LV_RES_OK;
    } else if (dsc->src_type == LV_IMG_SRC_VARIABLE) {
        SQOI *qoi = (SQOI *) dsc->user_data;
        int qoi_req_frame_index = y / qoi->qoi_single_frame_height;

        /*If line not from cache, refresh cache */
        if (qoi_req_frame_index != qoi->qoi_cache_frame_index) {
            qoi->io.raw_qoi_data = qoi->frame_base_array[ qoi_req_frame_index ];
            if (qoi_req_frame_index == (qoi->qoi_total_frames - 1)) {
                /*This is the last frame. */
                const uint32_t frame_offset = (uint32_t)(qoi->io.raw_qoi_data - qoi->qoi_data);
                qoi->io.raw_qoi_data_size = qoi->qoi_data_size - frame_offset;
            } else {
                qoi->io.raw_qoi_data_size =
                    (uint32_t)(qoi->frame_base_array[qoi_req_frame_index + 1] - qoi->io.raw_qoi_data);
            }

            uint32_t png_width;             /*No used, just required by he decoder*/
            uint32_t png_height;            /*No used, just required by he decoder*/

            /*Decode the image in ARGB8888 */
            error = qoi_decode32(&img_data, &png_width, &png_height, qoi->io.raw_qoi_data, qoi->io.raw_qoi_data_size);
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (qoi_decode32) error:%d", error);
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                convert_color_depth(img_data,  png_width * png_height);
                memcpy(qoi->frame_cache, img_data, qoi->qoi_single_frame_height * qoi->qoi_x_res * color_depth);
                if (img_data != NULL) {
                    free(img_data);
                }
            }
            qoi->qoi_cache_frame_index = qoi_req_frame_index;
        }

        uint8_t *cache = (uint8_t *)qoi->frame_cache + x * color_depth + (y % qoi->qoi_single_frame_height) * qoi->qoi_x_res * color_depth;
        memcpy(buf, cache, color_depth * len);
        return LV_RES_OK;
    }

    return LV_RES_INV;
}

static void decoder_close(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    LV_UNUSED(decoder);
    /*Free all allocated data*/
    SQOI *qoi = (SQOI *) dsc->user_data;
    if (!qoi) {
        return;
    }

    switch (dsc->src_type) {
    case LV_IMG_SRC_FILE:
        if (qoi->io.lv_file.file_d) {
            lv_fs_close(&(qoi->io.lv_file));
        }
        lv_qoi_cleanup(qoi);
        break;

    case LV_IMG_SRC_VARIABLE:
        lv_qoi_cleanup(qoi);
        break;

    default:
        ;
    }
}

/**
 * If the display is not in 32 bit format (ARGB888) then convert the image to the current color depth
 * @param img the ARGB888 image
 * @param px_cnt number of pixels in `img`
 */
static void convert_color_depth(uint8_t *img, uint32_t px_cnt)
{
#if LV_COLOR_DEPTH == 32
    lv_color32_t *img_argb = (lv_color32_t *)img;
    lv_color_t c;
    lv_color_t *img_c = (lv_color_t *) img;
    uint32_t i;
    for (i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.red, img_argb[i].ch.green, img_argb[i].ch.blue);
        img_c[i].ch.red = c.ch.blue;
        img_c[i].ch.blue = c.ch.red;
    }
#elif LV_COLOR_DEPTH == 16
    lv_color32_t *img_argb = (lv_color32_t *)img;
    lv_color_t c;
    uint32_t i;
    for (i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.blue, img_argb[i].ch.green, img_argb[i].ch.red);
        img[i * 3 + 2] = img_argb[i].ch.alpha;
        img[i * 3 + 1] = c.full >> 8;
        img[i * 3 + 0] = c.full & 0xFF;
    }
#elif LV_COLOR_DEPTH == 8
    lv_color32_t *img_argb = (lv_color32_t *)img;
    lv_color_t c;
    uint32_t i;
    for (i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.red, img_argb[i].ch.green, img_argb[i].ch.blue);
        img[i * 2 + 1] = img_argb[i].ch.alpha;
        img[i * 2 + 0] = c.full;
    }
#elif LV_COLOR_DEPTH == 1
    lv_color32_t *img_argb = (lv_color32_t *)img;
    uint8_t b;
    uint32_t i;
    for (i = 0; i < px_cnt; i++) {
        b = img_argb[i].ch.red | img_argb[i].ch.green | img_argb[i].ch.blue;
        img[i * 2 + 1] = img_argb[i].ch.alpha;
        img[i * 2 + 0] = b > 128 ? 1 : 0;
    }
#endif
}

static int is_qoi(const uint8_t *raw_data, size_t len)
{
    const uint8_t magic[] = {0x71, 0x6F, 0x69, 0x66};
    if (len < sizeof(magic)) {
        return false;
    }
    return memcmp(magic, raw_data, sizeof(magic)) == 0;
}

static void lv_qoi_free(SQOI *qoi)
{
    if (qoi->frame_cache) {
        free(qoi->frame_cache);
    }
    if (qoi->frame_base_array) {
        free(qoi->frame_base_array);
    }
    if (qoi->frame_base_offset) {
        free(qoi->frame_base_offset);
    }
}

static void lv_qoi_cleanup(SQOI *qoi)
{
    if (!qoi) {
        return;
    }

    lv_qoi_free(qoi);
    free(qoi);
}
