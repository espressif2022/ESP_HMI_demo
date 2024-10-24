[![Component Registry](https://components.espressif.com/components/espressif/esp_mmap_assets/badge.svg)](https://components.espressif.com/components/espressif/esp_mmap_assets)

## Instructions and Details

This module is primarily used for packaging assets (such as images, fonts, etc.) and directly mapping them for user access.

### Features

1. **Separation of Code and Assets**:
    - Assets are kept separate from the application code, reducing the size of the application binary and improving performance compared to using SPIFFS.

2. **Efficient Resource Management**:
    - Simplifies assets management by using automatically generated enums to access resource information.

3. **Memory-Efficient Image Decoding**:
    - Includes an image splitting script to reduce the memory required for image decoding.


## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency esp_mmap_assets
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Usage

### CMake
Optionally, users can opt to have the image automatically flashed together with the app binaries, partition tables, etc. on idf.py flash by specifying FLASH_IN_PROJECT. For example:
```c
    spiffs_create_partition_assets(my_spiffs_partition my_folder FLASH_IN_PROJECT)
```

### Initialization
```c
    mmap_assets_handle_t asset_handle;

    /* partitions.csv
    * --------------------------------------------------------
    * | Name               | Type | SubType | Offset | Size  | Flags     |
    * --------------------------------------------------------
    * | my_spiffs_partition | data | spiffs  |        | 6000K |           |
    * --------------------------------------------------------
    */
    const mmap_assets_config_t config = {
        .partition_label = "my_spiffs_partition",
        .max_files = MMAP_MY_FOLDER_FILES, //Get it from the compiled .h
        .checksum = MMAP_MY_FOLDER_CHECKSUM, //Get it from the compiled .h
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

    const char *name = mmap_assets_get_name(asset_handle, 0);
    const void *mem = mmap_assets_get_mem(asset_handle, 0);
    int size = mmap_assets_get_size(asset_handle, 0);
    int width = mmap_assets_get_width(asset_handle, 0);
    int height = mmap_assets_get_height(asset_handle, 0);

    ESP_LOGI(TAG, "Asset - Name:[%s], Memory:[%p], Size:[%d bytes], Width:[%d px], Height:[%d px]", name, mem, size, width, height);

```
