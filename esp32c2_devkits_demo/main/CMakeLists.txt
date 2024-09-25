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

spiffs_create_partition_assets(assets ../spiffs_assets FLASH_IN_PROJECT)
