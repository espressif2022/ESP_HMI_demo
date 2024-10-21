[![Component Registry](https://components.espressif.com/components/espressif/esp_lv_spng/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_spng)

## Instructions and Details

Allow the use of PNG images in LVGL. Besides that it also allows the use of a custom format, called Split PNG (SPNG), which can be decoded in more optimal way on embedded systems.

[Referencing the implementation of SJPG.](https://docs.lvgl.io/8.4/libs/sjpg.html#overview)

### Features
    - Supports both standard PNG and custom SPNG formats.

    - Decoding standard PNG requires RAM equivalent to the full uncompressed image size (recommended for devices with more RAM).

    - SPNG is a custom format based on standard PNG, specifically designed for LVGL.

    - SPNG is a 'split-png' format comprising small PNG fragments and an SPNG header.

    - File read from file and c-array are implemented.

    - SPNG images are decoded in segments, so zooming and rotating are not supported.


## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency esp_lv_spng
```

## Usage

### Converting PNG to SPNG
The [esp_mmap_assets](https://components.espressif.com/components/espressif/esp_mmap_assets) component is required. It will automatically package and convert PNG images to SPNG format during compilation.
```c
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_SPNG
        MMAP_SPLIT_HEIGHT 16)

    [9/20] Move and Pack assets...
    --support_format: ['.png']
    --support_spng: True
    --support_sjpg: False
    --support_qoi: False
    --support_raw: False
    --split_height: 16
    Completed navi_52.png -> navi_52.spng
```

### Initialization
```c
    esp_lv_spng_decoder_handle_t spng_handle = NULL;
    esp_lv_split_png_init(&spng_handle); //Initialize this after lvgl starts
```
