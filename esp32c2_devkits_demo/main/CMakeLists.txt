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

# spiffs_create_partition_assets(
#     assets_B
#     ${Drive_B}
#     FLASH_IN_PROJECT
#     MMAP_FILE_SUPPORT_FORMAT ".png,.jpg"
#     MMAP_SUPPORT_SPNG
#     MMAP_SPLIT_HEIGHT 4
# )

# usage: LVGLImage.py            [-h]
#                                [--ofmt {C,BIN,PNG}]
#                                [--cf {L8,I1,I2,I4,I8,A1,A2,A4,A8,ARGB8888,XRGB8888,
#                                        RGB565,RGB565A8,ARGB8565,RGB888,AUTO,RAW,RAW_ALPHA}]
#                                [--premultiply]
#                                [--compress {NONE,RLE,LZ4}]
#                                [--align [byte]]
#                                [--background [color]]
#                                [-o OUTPUT]
#                                [-v]
#                                input

# positional arguments:
#   input                     the filename or folder to be recursively converted

# optional arguments:
#   -h, --help                show this help message and exit
#   --ofmt {C,BIN,PNG}        output filename format (default: BIN)
#   --cf {L8,I1,I2,I4,I8,A1,A2,A4,A8,ARGB8888,XRGB8888,
#            RGB565,RGB565A8,ARGB8565,RGB888,AUTO,RAW,RAW_ALPHA}
#                             bin image color format, use AUTO to automatically choose from I1/2/4/8
#                             (default: I8)
#   --premultiply             pre-multiply color with alpha (default: False)
#   --compress {NONE,RLE,LZ4} binary data compression method (default: NONE)
#   --align [byte]            stride alignment in bytes for bin image (default: 1)
#   --background [color]      background color for formats without alpha (default: 0x000000)
#   -o OUTPUT, --output OUTPUT
#                             select the output folder (default: ./output)
#   -v, --verbose             increase output verbosity


# usage: lv_img_conv.py [-h] [-f {true_color,true_color_alpha,true_color_chroma,indexed_1,indexed_2,indexed_4,indexed_8,alpha_1,alpha_2,alpha_4,alpha_8,raw,raw_alpha,raw_chroma}]
#                       [-cf {RGB332,RGB565,RGB565SWAP,RGB888}] [-ff {C,BIN}] [-o O] [-r] [-d]
#                       filepath [filepath ...]

# positional arguments:
#   filepath              images dir paths (or file paths) you wanna convert

# options:
#   -h, --help            show this help message and exit
#   -f {true_color,true_color_alpha,true_color_chroma,indexed_1,indexed_2,indexed_4,indexed_8,alpha_1,alpha_2,alpha_4,alpha_8,raw,raw_alpha,raw_chroma}, -format {true_color,true_color_alpha,true_color_chroma,indexed_1,indexed_2,indexed_4,indexed_8,alpha_1,alpha_2,alpha_4,alpha_8,raw,raw_alpha,raw_chroma}
#                         converted file format: true_color, true_color_alpha, true_color_chroma, indexed_1, indexed_2, indexed_4, indexed_8, alpha_1, alpha_2, alpha_4, alpha_8, raw, raw_alpha,
#                         raw_chroma. The default is: true_color
#   -cf {RGB332,RGB565,RGB565SWAP,RGB888}, -color-format {RGB332,RGB565,RGB565SWAP,RGB888}
#                         converted color format: RGB332, RGB565, RGB565SWAP, RGB888
#   -ff {C,BIN}, -file-format {C,BIN}
#                         converted file format: C(*.c), BIN(*.bin)
#   -o O, -output-filepath O
#                         output file path. if not set, it will saved in the input dir
#   -r                    convert files recursively
#   -d                    need to dith


spiffs_create_partition_assets(
    assets_B
    ${Drive_B}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png,.jpg"
    MMAP_SUPPORT_RAW
    MMAP_RAW_DITHER
    MMAP_RAW_BGR_MODE
    MMAP_RAW_FILE_FORMAT "true_color_alpha"
    MMAP_RAW_COLOR_FORMAT "RGB565"
)
