[![Component Registry](https://components.espressif.com/components/espressif/esp_lv_sjpg/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_sjpg)

## Instructions and Details

Allow the use of JPG images in LVGL. Besides that it also allows the use of a custom format, called Split JPG (SJPG), which can be decoded in more optimal way on embedded systems.

[Referencing the implementation of SJPG.](https://docs.lvgl.io/8.4/libs/sjpg.html#overview)

### Features
    - Supports both standard JPG and custom SJPG formats.

    - Decoding standard JPG requires RAM equivalent to the full uncompressed image size (recommended for devices with more RAM).

    - SJPG is a custom format based on standard JPG, specifically designed for LVGL.

    - SJPG is a 'split-jpg' format comprising small JPG fragments and an SJPG header.

    - File read from file and c-array are implemented.

    - SJPG images are decoded in segments, so zooming and rotating are not supported.


## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency esp_lv_sjpg
```

## Usage

### Converting JPG to SJPG
The [esp_mmap_assets](https://components.espressif.com/components/espressif/esp_mmap_assets) component is required. It will automatically package and convert JPG images to SJPG format during compilation.
```c
    [12/1448] Move and Pack assets...
    --support_format: .jpg,.png
    --support_spng: ON
    --support_sjpg: ON
    --split_height: 16
    Input: temp_icon.jpg    RES: 90 x 90    splits: 6
    Completed, saved as: temp_icon.sjpg
```

### Initialization
```c
    esp_lv_sjpg_decoder_handle_t sjpg_handle = NULL;
    esp_lv_split_jpg_init(&sjpg_handle); //Initialize this after lvgl starts
```
