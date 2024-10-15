idf_component_register(
    SRCS "esp_lv_sjpg.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
)

add_prebuilt_library(esp_jpeg "${CMAKE_CURRENT_SOURCE_DIR}/lib/${CONFIG_IDF_TARGET}/libesp_jpeg.a")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--start-group" esp_jpeg "-Wl,--end-group")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
