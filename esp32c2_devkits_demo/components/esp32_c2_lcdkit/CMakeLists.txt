if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "5.0")
    set(SRC_VER "esp32_lcdkit_idf4.c")
else()
    set(SRC_VER "esp32_lcdkit_idf5.c")
endif()

idf_component_register(
    SRCS "esp32_c2_lcdkit.c" ${SRC_VER}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    PRIV_REQUIRES
        "esp_lcd"
    REQUIRES
        "driver"
        "spiffs"
)