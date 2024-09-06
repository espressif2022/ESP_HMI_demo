SET(SOURCES screens/ui_systemstart_screen.c
    screens/ui_systemclose_screen.c
    screens/ui_home_screen.c
    screens/ui_brushing_screen.c
    screens/ui_evaluation_screen.c
    screens/ui_charging_screen.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_esplogo_png.c
    images/ui_img_power_off_png.c
    images/ui_img_wifion_png.c
    images/ui_img_bt_off_png.c
    images/ui_img_tooth_logox_png.c
    images/ui_img_battery8_png.c
    images/ui_img_start_brush_icon_png.c
    images/ui_img_effect1_png.c
    images/ui_img_power_icon_png.c
    images/ui_img_bt_on_png.c
    images/ui_img_effect2_png.c
    images/ui_img_effect3_png.c
    images/ui_img_effect4_png.c
    images/ui_img_wifioff_png.c
    images/ui_img_stop_brush_icon_png.c
    images/ui_img_effect0_png.c
    images/ui_img_bat_container_png.c)

add_library(ui ${SOURCES})
