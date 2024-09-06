
idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS ".")

set(SOURCE_DIR "../test_assets")
set(Drive_A "${CMAKE_BINARY_DIR}/Drive_A")
set(Drive_B "${CMAKE_BINARY_DIR}/Drive_B")
set(Drive_C "${CMAKE_BINARY_DIR}/Drive_C")

file(MAKE_DIRECTORY ${Drive_A})
file(MAKE_DIRECTORY ${Drive_B})
file(MAKE_DIRECTORY ${Drive_C})

file(GLOB_RECURSE SOURCE_FILES ${SOURCE_DIR}/*)
foreach(FILE ${SOURCE_FILES})
    file(COPY ${FILE} DESTINATION ${Drive_A})
    file(COPY ${FILE} DESTINATION ${Drive_B})
    file(COPY ${FILE} DESTINATION ${Drive_C})
endforeach()

spiffs_create_partition_assets(assets_A ${Drive_A} FLASH_IN_PROJECT)
spiffs_create_partition_assets(assets_B ${Drive_B} FLASH_IN_PROJECT)
spiffs_create_partition_image(assets_C ${Drive_C} FLASH_IN_PROJECT)
