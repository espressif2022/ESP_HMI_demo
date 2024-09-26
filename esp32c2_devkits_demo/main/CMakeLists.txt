idf_component_register(
    SRC_DIRS
        "."
        # "0_96_UI"
        "1_28_UI"
        "1_28_UI/assets"
        # "rainmaker"
        # "gui"
        # "gui/toothbrush_ui"
        # "gui/toothbrush_ui/screens"
        # "gui/toothbrush_ui/images"
    INCLUDE_DIRS
        "."
        # "0_96_UI"
        # "1_28_UI"
        # "rainmaker"
        # "gui"
        # "gui/toothbrush_ui"
        # "gui/toothbrush_ui/screens"
        # "gui/toothbrush_ui/images"
        )

# spiffs_create_partition_assets(assets ../spiffs_assets FLASH_IN_PROJECT)

set(SOURCE_DIR "../spiffs_assets")
set(Drive_A "${CMAKE_BINARY_DIR}/Drive_A")
set(Drive_B "${CMAKE_BINARY_DIR}/Drive_B")

file(MAKE_DIRECTORY ${Drive_A})
file(MAKE_DIRECTORY ${Drive_B})

file(GLOB_RECURSE SOURCE_FILES ${SOURCE_DIR}/*)
foreach(FILE ${SOURCE_FILES})
    file(COPY ${FILE} DESTINATION ${Drive_A})
    file(COPY ${FILE} DESTINATION ${Drive_B})
endforeach()

spiffs_create_partition_assets(
    assets_A
    ${Drive_A}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png,.jpg"
    MMAP_SUPPORT_QOI
    MMAP_SUPPORT_SQOI
    MMAP_SPLIT_HEIGHT 4
)

spiffs_create_partition_assets(
    assets_B
    ${Drive_B}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png,.jpg"
    MMAP_SUPPORT_SPNG
    MMAP_SPLIT_HEIGHT 4
)
