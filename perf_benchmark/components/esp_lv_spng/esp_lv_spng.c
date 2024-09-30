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
#include "esp_lv_spng.h"

#include "lvgl.h"
#include "png.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    uint8_t *raw_spng_data;             //Used when type==SPNG_IO_SOURCE_C_ARRAY.
    uint32_t raw_spng_data_size;        //Num bytes pointed to by raw_spng_data.
    lv_fs_file_t lv_file;
} io_source_t;

typedef struct {
    uint8_t *spng_data;
    uint32_t spng_data_size;
    int spng_x_res;
    int spng_y_res;
    int spng_total_frames;
    int spng_single_frame_height;
    int spng_cache_frame_index;
    uint8_t **frame_base_array;        //to save base address of each split frames upto spng_total_frames.
    int * frame_base_offset;            //to save base offset for fseek
    uint8_t *frame_cache;
    io_source_t io;
} SPNG;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t decoder_info(struct _lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header);
static lv_res_t decoder_open(lv_img_decoder_t *dec, lv_img_decoder_dsc_t *dsc);
static lv_res_t decoder_read_line(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t len, uint8_t *buf);
static void decoder_close(lv_img_decoder_t *dec, lv_img_decoder_dsc_t *dsc);
static void convert_color_depth(uint8_t *img, uint32_t px_cnt);
static int is_png(const uint8_t *raw_data, size_t len);
static void lv_spng_cleanup(SPNG *spng);
static void lv_spng_free(SPNG *spng);

static lv_res_t libpng_decode32(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, size_t insize);

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "spng";

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Register the PNG decoder functions in LVGL
 */
esp_err_t esp_lv_split_png_init(esp_lv_spng_decoder_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    lv_img_decoder_t *dec = lv_img_decoder_create();
    lv_img_decoder_set_info_cb(dec, decoder_info);
    lv_img_decoder_set_open_cb(dec, decoder_open);
    lv_img_decoder_set_close_cb(dec, decoder_close);
    lv_img_decoder_set_read_line_cb(dec, decoder_read_line);

    *ret_handle = dec;
    ESP_LOGD(TAG, "new spng decoder @%p", dec);

    ESP_LOGD(TAG, "spng decoder create success, version: %d.%d.%d", ESP_LV_SPNG_VER_MAJOR, ESP_LV_SPNG_VER_MINOR, ESP_LV_SPNG_VER_PATCH);
    return ESP_OK;
}

esp_err_t esp_lv_split_png_deinit(esp_lv_spng_decoder_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid decoder handle pointer");
    ESP_LOGD(TAG, "delete spng decoder @%p", handle);
    lv_img_decoder_delete(handle);

    return ESP_OK;

}

/**********************
 *   STATIC FUNCTIONS
 **********************/

lv_res_t libpng_decode32(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, size_t insize)
{
    if (!in || !out || !w || !h) {
        return LV_RES_INV;
    }

    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_memory(&image, in, insize)) {
        return LV_RES_INV;
    }

    image.format = PNG_FORMAT_RGBA;

    *w = image.width;
    *h = image.height;
    *out = malloc(PNG_IMAGE_SIZE(image));
    if (*out == NULL) {
        png_image_free(&image);
        return LV_RES_INV;
    }

    if (!png_image_finish_read(&image, NULL, *out, 0, NULL)) {
        free(*out);
        png_image_free(&image);
        return LV_RES_INV;
    }

    return LV_RES_OK;
}

static lv_fs_res_t png_load_file(const char *filename, uint8_t **buffer, size_t *size, bool read_head)
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
        uint8_t *raw_spng_data = (uint8_t *)img_dsc->data;
        const uint32_t data_size = img_dsc->data_size;
        const uint32_t *size = ((uint32_t *)img_dsc->data) + 4;

        if (!strncmp((char *)raw_spng_data, "_SPNG__", strlen("_SPNG__"))) {
            raw_spng_data += 14; //seek to res info ... refer spng format
            header->always_zero = 0;
            header->cf = LV_IMG_CF_RAW_ALPHA;

            header->w = *raw_spng_data++;
            header->w |= *raw_spng_data++ << 8;

            header->h = *raw_spng_data++;
            header->h |= *raw_spng_data++ << 8;

            return lv_ret;
        } else if (is_png(raw_spng_data, data_size) == true) {
            header->always_zero = 0;

            if (img_dsc->header.cf) {
                header->cf = img_dsc->header.cf;       /*Save the color format*/
            } else {
                header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            }

            if (img_dsc->header.w) {
                header->w = img_dsc->header.w;         /*Save the image width*/
            } else {
                header->w = (lv_coord_t)((size[0] & 0xff000000) >> 24) + ((size[0] & 0x00ff0000) >> 8);
            }

            if (img_dsc->header.h) {
                header->h = img_dsc->header.h;         /*Save the color height*/
            } else {
                header->h = (lv_coord_t)((size[1] & 0xff000000) >> 24) + ((size[1] & 0x00ff0000) >> 8);
            }

            return lv_ret;
        } else {
            return LV_RES_INV;
        }
    } else if (src_type == LV_IMG_SRC_FILE) {
        const char *fn = src;
        if (strcmp(lv_fs_get_ext(fn), "png") == 0) {
            uint8_t *png_data = NULL;       /*Pointer to the loaded data. Same as the original file just loaded into the RAM*/
            size_t spng_data_size;          /*Size of `png_data` in bytes*/

            if (png_load_file(fn, &png_data, &spng_data_size, true) != LV_FS_RES_OK) {
                if (png_data) {
                    free(png_data);
                }
                return LV_RES_INV;
            }

            const uint32_t *size = ((uint32_t *)png_data) + 4;
            header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            header->w = (lv_coord_t)((size[0] & 0xff000000) >> 24) + ((size[0] & 0x00ff0000) >> 8);
            header->h = (lv_coord_t)((size[1] & 0xff000000) >> 24) + ((size[1] & 0x00ff0000) >> 8);
            free(png_data);

        } else if (strcmp(lv_fs_get_ext(fn), "spng") == 0) {

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

            if (strcmp((char *)buff, "_SPNG__") == 0) {
                lv_fs_seek(&file, 14, LV_FS_SEEK_SET);
                res = lv_fs_read(&file, buff, 4, &rn);
                if (res != LV_FS_RES_OK || rn != 4) {
                    lv_fs_close(&file);
                    return LV_RES_INV;
                }
                header->always_zero = 0;
                header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
                uint8_t * raw_png_data = buff;
                header->w = *raw_png_data++;
                header->w |= *raw_png_data++ << 8;
                header->h = *raw_png_data++;
                header->h |= *raw_png_data++ << 8;
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
        SPNG *spng = (SPNG *) dsc->user_data;
        const uint32_t raw_spng_data_size = ((lv_img_dsc_t *)dsc->src)->data_size;
        if (spng == NULL) {
            spng =  malloc(sizeof(SPNG));
            if (!spng) {
                return LV_RES_INV;
            }

            memset(spng, 0, sizeof(SPNG));

            dsc->user_data = spng;
            spng->spng_data = (uint8_t *)((lv_img_dsc_t *)(dsc->src))->data;
            spng->spng_data_size = ((lv_img_dsc_t *)(dsc->src))->data_size;
        }

        if (!strncmp((char *) spng->spng_data, "_SPNG__", strlen("_SPNG__"))) {
            data = spng->spng_data;
            data += 14;

            spng->spng_x_res = *data++;
            spng->spng_x_res |= *data++ << 8;

            spng->spng_y_res = *data++;
            spng->spng_y_res |= *data++ << 8;

            spng->spng_total_frames = *data++;
            spng->spng_total_frames |= *data++ << 8;

            spng->spng_single_frame_height = *data++;
            spng->spng_single_frame_height |= *data++ << 8;

            ESP_LOGD(TAG, "[%d,%d], frames:%d, height:%d", spng->spng_x_res, spng->spng_y_res, \
                     spng->spng_total_frames, spng->spng_single_frame_height);
            spng->frame_base_array = malloc(sizeof(uint8_t *) * spng->spng_total_frames);
            if (! spng->frame_base_array) {
                ESP_LOGE(TAG, "Not enough memory for frame_base_array allocation");
                lv_spng_cleanup(spng);
                spng = NULL;
                return LV_RES_INV;
            }

            uint8_t *img_frame_base = data +  spng->spng_total_frames * 2;
            spng->frame_base_array[0] = img_frame_base;

            for (int i = 1; i <  spng->spng_total_frames; i++) {
                int offset = *data++;
                offset |= *data++ << 8;
                spng->frame_base_array[i] = spng->frame_base_array[i - 1] + offset;
            }
            spng->spng_cache_frame_index = -1;
            spng->frame_cache = (void *)malloc(spng->spng_x_res * spng->spng_single_frame_height * 4);
            if (! spng->frame_cache) {
                ESP_LOGE(TAG, "Not enough memory for frame_cache allocation");
                lv_spng_cleanup(spng);
                spng = NULL;
                return LV_RES_INV;
            }
            dsc->img_data = NULL;

            return lv_ret;
        } else if (is_png(spng->spng_data, raw_spng_data_size) == true) {
            /*Decode the image in ARGB8888 */
            lv_ret = libpng_decode32(&img_data, &png_width, &png_height, img_dsc->data, img_dsc->data_size);
            if (lv_ret != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (libpng_decode32) error:%d", lv_ret);
                if (img_data != NULL) {
                    free(img_data);
                }
                lv_spng_cleanup(spng);
                spng = NULL;
                return LV_RES_INV;
            } else {
                /*Convert the image to the system's color depth*/
                convert_color_depth(img_data,  png_width * png_height);
                dsc->img_data = img_data;
                spng->frame_cache = img_data;
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

        if (strcmp(lv_fs_get_ext(fn), "png") == 0) {
            uint8_t *png_data = NULL;   /*Pointer to the loaded data. Same as the original file just loaded into the RAM*/
            size_t spng_data_size;       /*Size of `png_data` in bytes*/

            SPNG *spng = (SPNG *) dsc->user_data;
            if (spng == NULL) {
                spng = malloc(sizeof(SPNG));
                if (!spng) {
                    ESP_LOGE(TAG, "Failed to allocate memory for png");
                    return LV_RES_INV;
                }

                memset(spng, 0, sizeof(SPNG));
                dsc->user_data = spng;
            }

            if (png_load_file(fn, &png_data, &spng_data_size, false) != LV_FS_RES_OK) {
                if (png_data) {
                    free(png_data);
                    lv_spng_cleanup(spng);
                }
                return LV_RES_INV;
            }

            lv_ret = libpng_decode32(&img_data, &png_width, &png_height, png_data, spng_data_size);
            free(png_data);
            if (lv_ret != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (libpng_decode32) error:%d", lv_ret);
                if (img_data != NULL) {
                    free(img_data);
                }
                lv_spng_cleanup(spng);
                spng = NULL;
                return LV_RES_INV;
            } else {
                /*Convert the image to the system's color depth*/
                convert_color_depth(img_data,  png_width * png_height);
                dsc->img_data = img_data;
                spng->frame_cache = img_data;
            }
            return lv_ret;
        } else if (strcmp(lv_fs_get_ext(fn), "spng") == 0) {
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

            if (strcmp((char *)buff, "_SPNG__") == 0) {

                SPNG *spng = (SPNG *)dsc->user_data;
                if (spng == NULL) {
                    spng = malloc(sizeof(SPNG));
                    if (! spng) {
                        ESP_LOGI(TAG, "Failed to allocate memory for spng");
                        lv_fs_close(&lv_file);
                        return LV_RES_INV;
                    }
                    memset(spng, 0, sizeof(SPNG));
                    dsc->user_data = spng;
                }
                data = buff;
                data += 14;

                spng->spng_x_res = *data++;
                spng->spng_x_res |= *data++ << 8;

                spng->spng_y_res = *data++;
                spng->spng_y_res |= *data++ << 8;

                spng->spng_total_frames = *data++;
                spng->spng_total_frames |= *data++ << 8;

                spng->spng_single_frame_height = *data++;
                spng->spng_single_frame_height |= *data++ << 8;

                ESP_LOGD(TAG, "[%d,%d], frames:%d, height:%d", spng->spng_x_res, spng->spng_y_res, \
                         spng->spng_total_frames, spng->spng_single_frame_height);
                spng->frame_base_offset = malloc(sizeof(int *) * spng->spng_total_frames);
                if (! spng->frame_base_offset) {
                    ESP_LOGE(TAG, "Not enough memory for frame_base_offset allocation");
                    lv_spng_cleanup(spng);
                    spng = NULL;
                    return LV_RES_INV;
                }

                int img_frame_start_offset = 22 +  spng->spng_total_frames * 2;
                spng->frame_base_offset[0] = img_frame_start_offset;

                for (int i = 1; i <  spng->spng_total_frames; i++) {
                    res = lv_fs_read(&lv_file, buff, 2, &rn);
                    if (res != LV_FS_RES_OK || rn != 2) {
                        lv_fs_close(&lv_file);
                        return LV_RES_INV;
                    }

                    data = buff;
                    int offset = *data++;
                    offset |= *data++ << 8;
                    spng->frame_base_offset[i] = spng->frame_base_offset[i - 1] + offset;
                }

                spng->spng_cache_frame_index = -1; //INVALID AT BEGINNING for a forced compare mismatch at first time.
                spng->frame_cache = (void *)malloc(spng->spng_x_res * spng->spng_single_frame_height * 4);
                if (!spng->frame_cache) {
                    ESP_LOGE(TAG, "Not enough memory for frame_cache allocation");
                    lv_fs_close(&lv_file);
                    lv_spng_cleanup(spng);
                    spng = NULL;
                    return LV_RES_INV;
                }

                uint32_t total_size;
                lv_fs_seek(&lv_file, 0, LV_FS_SEEK_END);
                lv_fs_tell(&lv_file, &total_size);
                spng->spng_data_size = total_size;

                spng->io.lv_file = lv_file;
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

        SPNG *spng = (SPNG *) dsc->user_data;

        lv_fs_file_t *lv_file_p = &(spng->io.lv_file);
        if (!lv_file_p) {
            ESP_LOGI(TAG, "lv_img_decoder_read_line: lv_file_p");
            return LV_RES_INV;
        }

        int png_req_frame_index = y / spng->spng_single_frame_height;
        /*If line not from cache, refresh cache */
        if (png_req_frame_index != spng->spng_cache_frame_index) {

            if (png_req_frame_index == (spng->spng_total_frames - 1)) {
                /*This is the last frame. */
                spng->io.raw_spng_data_size = spng->spng_data_size - (uint32_t)(spng->frame_base_offset[png_req_frame_index]);
            } else {
                spng->io.raw_spng_data_size =
                    (uint32_t)(spng->frame_base_offset[png_req_frame_index + 1] - (uint32_t)(spng->frame_base_offset[png_req_frame_index]));
            }

            int next_read_pos = (int)(spng->frame_base_offset [ png_req_frame_index ]);
            lv_fs_seek(lv_file_p, next_read_pos, LV_FS_SEEK_SET);
            res = lv_fs_read(lv_file_p, spng->frame_cache, spng->io.raw_spng_data_size, &rn);
            if (res != LV_FS_RES_OK || rn != spng->io.raw_spng_data_size) {
                lv_fs_close(lv_file_p);
                return LV_RES_INV;
            }

            uint32_t png_width;             /*No used, just required by he decoder*/
            uint32_t png_height;            /*No used, just required by he decoder*/

            /*Decode the image in ARGB8888 */
            error = libpng_decode32(&img_data, &png_width, &png_height, spng->frame_cache, rn);
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (libpng_decode32) error:%d", error);
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                convert_color_depth(img_data,  png_width * png_height);
                memcpy(spng->frame_cache, img_data, png_width * png_height * color_depth);
                if (img_data != NULL) {
                    free(img_data);
                }
            }
            spng->spng_cache_frame_index = png_req_frame_index;
        }

        uint8_t *cache = (uint8_t *)spng->frame_cache + x * color_depth + (y % spng->spng_single_frame_height) * spng->spng_x_res * color_depth;
        memcpy(buf, cache, color_depth * len);
        return LV_RES_OK;
    } else if (dsc->src_type == LV_IMG_SRC_VARIABLE) {
        SPNG *spng = (SPNG *) dsc->user_data;

        int spng_req_frame_index = y / spng->spng_single_frame_height;

        /*If line not from cache, refresh cache */
        if (spng_req_frame_index != spng->spng_cache_frame_index) {
            spng->io.raw_spng_data = spng->frame_base_array[ spng_req_frame_index ];
            if (spng_req_frame_index == (spng->spng_total_frames - 1)) {
                /*This is the last frame. */
                const uint32_t frame_offset = (uint32_t)(spng->io.raw_spng_data - spng->spng_data);
                spng->io.raw_spng_data_size = spng->spng_data_size - frame_offset;
            } else {
                spng->io.raw_spng_data_size =
                    (uint32_t)(spng->frame_base_array[spng_req_frame_index + 1] - spng->io.raw_spng_data);
            }

            uint32_t png_width;             /*No used, just required by he decoder*/
            uint32_t png_height;            /*No used, just required by he decoder*/

            /*Decode the image in ARGB8888 */
            error = libpng_decode32(&img_data, &png_width, &png_height, spng->io.raw_spng_data, spng->io.raw_spng_data_size);
            if (error != LV_RES_OK) {
                ESP_LOGE(TAG, "Decode (libpng_decode32) error:%d", error);
                if (img_data != NULL) {
                    free(img_data);
                }
                return LV_RES_INV;
            } else {
                convert_color_depth(img_data,  png_width * png_height);
                memcpy(spng->frame_cache, img_data, spng->spng_single_frame_height * spng->spng_x_res * color_depth);
                if (img_data != NULL) {
                    free(img_data);
                }
            }
            spng->spng_cache_frame_index = spng_req_frame_index;
        }

        uint8_t *cache = (uint8_t *)spng->frame_cache + x * color_depth + (y % spng->spng_single_frame_height) * spng->spng_x_res * color_depth;
        memcpy(buf, cache, color_depth * len);
        return LV_RES_OK;
    }

    return LV_RES_INV;
}

static void decoder_close(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    LV_UNUSED(decoder);
    /*Free all allocated data*/
    SPNG *spng = (SPNG *) dsc->user_data;
    if (!spng) {
        return;
    }

    switch (dsc->src_type) {
    case LV_IMG_SRC_FILE:
        if (spng->io.lv_file.file_d) {
            lv_fs_close(&(spng->io.lv_file));
        }
        lv_spng_cleanup(spng);
        break;

    case LV_IMG_SRC_VARIABLE:
        lv_spng_cleanup(spng);
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

static int is_png(const uint8_t *raw_data, size_t len)
{
    const uint8_t magic[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (len < sizeof(magic)) {
        return false;
    }
    return memcmp(magic, raw_data, sizeof(magic)) == 0;
}

static void lv_spng_free(SPNG *spng)
{
    if (spng->frame_cache) {
        free(spng->frame_cache);
    }
    if (spng->frame_base_array) {
        free(spng->frame_base_array);
    }
    if (spng->frame_base_offset) {
        free(spng->frame_base_offset);
    }
}

static void lv_spng_cleanup(SPNG *spng)
{
    if (! spng) {
        return;
    }

    lv_spng_free(spng);
    free(spng);
}
