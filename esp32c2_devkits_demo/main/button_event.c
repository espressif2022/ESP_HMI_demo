
#include "lv_example_pub.h"
#include "iot_button.h"

#define BUTTON_ACTIVE_LEVEL     0


extern lv_obj_t *g_menu_page;       // main/ui/ui_menu_new.c
extern lv_obj_t *g_wakeup_page;     // main/ui/ui_clockScreen.c
extern lv_obj_t *g_wash_page;       // main/ui/ui_wash.c
extern lv_obj_t *g_light_page;      // main/ui/ui_light_2color.c
extern lv_obj_t *g_thermostat_page; // main/ui/ui_thermostat.c
bool get_sleep_ui_status(void);     // main/ui/ui_clockScreen.c
bool get_wash_ui_mode(void);        // main/ui/ui_wash.c

typedef enum {
    BUTTON_SINGLE_CLICK_RIGHT = 0,
    BUTTON_SINGLE_CLICK_CENTER,
    BUTTON_SINGLE_CLICK_LEFT,
    BUTTON_DOUBLE_CLICK_CENTER_LONG_PRESS,
}bsp_button_event_t;

typedef enum {
    MAIN_MENU = 0,
    WASH_MENU,
    LIGHT_MENU,
    THERMOSTAT_MENU,
} menu_status_t;

typedef enum {
    MAIN_WASH_MENU = 1,
    MAIN_LIGHT_MENU,
    MAIN_THERMOSTAT_MENU,
} main_menu_t;

typedef struct {
    menu_status_t menu_status;
    main_menu_t    main_menu_status;
} button_status_t;

static button_status_t button_status = {
    .menu_status = MAIN_MENU,
    .main_menu_status = MAIN_WASH_MENU,
};

static const char *TAG = "button_event";

static void main_menu_control(int data)
{
    uint32_t key;
    switch ((int)data) {
    case BUTTON_SINGLE_CLICK_RIGHT:
        if (button_status.main_menu_status == MAIN_THERMOSTAT_MENU) {
            button_status.main_menu_status = MAIN_WASH_MENU;
        } else {
            button_status.main_menu_status++;
        }
        ESP_LOGD(TAG, "main_menu_status %d", button_status.main_menu_status);
        key = LV_KEY_RIGHT;
        lv_event_send(g_menu_page, LV_EVENT_KEY, (void *)&key);
        break;
    case BUTTON_SINGLE_CLICK_CENTER:
        lv_event_send(g_menu_page, LV_EVENT_CLICKED, NULL);
        button_status.menu_status = button_status.main_menu_status;
        break;
    case BUTTON_SINGLE_CLICK_LEFT:
        if (button_status.main_menu_status == MAIN_WASH_MENU) {
            button_status.main_menu_status = MAIN_THERMOSTAT_MENU;
        } else {
            button_status.main_menu_status--;
        }
        ESP_LOGD(TAG, "main_menu_status %d", button_status.main_menu_status);
        key = LV_KEY_LEFT;
        lv_event_send(g_menu_page, LV_EVENT_KEY, (void *)&key);
        break;
    default:
        break;
    }
}

static void sleep_menu_control(int data)
{
    if (data == BUTTON_DOUBLE_CLICK_CENTER_LONG_PRESS) {
        bsp_display_lock(-1);
        lv_event_send(g_wakeup_page, LV_EVENT_CLICKED, NULL);
        bsp_display_unlock();
    }
}

static void wash_menu_control(int data)
{
    uint32_t key;
    switch (data) {
    case BUTTON_SINGLE_CLICK_RIGHT:
        key = LV_KEY_LEFT;
        lv_event_send(g_wash_page, LV_EVENT_KEY, (void *)&key);
        break;
    case BUTTON_SINGLE_CLICK_CENTER:
        lv_event_send(g_wash_page, LV_EVENT_CLICKED, NULL);
        break;
    case BUTTON_SINGLE_CLICK_LEFT:
        key = LV_KEY_RIGHT;
        lv_event_send(g_wash_page, LV_EVENT_KEY, (void *)&key);
        break;
    case BUTTON_DOUBLE_CLICK_CENTER_LONG_PRESS:
        if (get_wash_ui_mode() == true) {
            button_status.menu_status = MAIN_MENU;
        } 
        lv_event_send(g_wash_page, LV_EVENT_LONG_PRESSED, NULL);
        break;
    default:
        break;
    }
}

static void light_menu_control(int data)
{
    uint32_t key;
    switch (data) {
    case BUTTON_SINGLE_CLICK_RIGHT:
        key = LV_KEY_LEFT;
        lv_event_send(g_light_page, LV_EVENT_KEY, (void *)&key);
        break;
    case BUTTON_SINGLE_CLICK_CENTER:
        lv_event_send(g_light_page, LV_EVENT_CLICKED, NULL);
        break;
    case BUTTON_SINGLE_CLICK_LEFT:
        key = LV_KEY_RIGHT;
        lv_event_send(g_light_page, LV_EVENT_KEY, (void *)&key);
        break;
    case BUTTON_DOUBLE_CLICK_CENTER_LONG_PRESS:
        lv_event_send(g_light_page, LV_EVENT_LONG_PRESSED, NULL);
        button_status.menu_status = MAIN_MENU;
        break;
    default:
        break;
    }
}

static void thermostat_menu_control(int data)
{
    uint32_t key;
    switch (data) {
    case BUTTON_SINGLE_CLICK_RIGHT:
        key = LV_KEY_RIGHT;
        lv_event_send(g_thermostat_page, LV_EVENT_KEY, (void *)&key);
        break;
    case BUTTON_SINGLE_CLICK_LEFT:
        key = LV_KEY_LEFT;
        lv_event_send(g_thermostat_page, LV_EVENT_KEY, (void *)&key);
        break;
    case BUTTON_DOUBLE_CLICK_CENTER_LONG_PRESS:
        lv_event_send(g_thermostat_page, LV_EVENT_LONG_PRESSED, NULL);
        button_status.menu_status = MAIN_MENU;
        break;
    default:
        break;
    }
}

static void button_event_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "button event %d,button_status.menu_status %d", (int)data, button_status.menu_status);

    if (get_sleep_ui_status() == true) {
        sleep_menu_control((int)data);
    } else {
        switch (button_status.menu_status) {
        case MAIN_MENU:
            main_menu_control((int)data);
            break;
        case WASH_MENU:
            wash_menu_control((int)data);
            break;
        case LIGHT_MENU:
            light_menu_control((int)data);
            break;
        case THERMOSTAT_MENU:
            thermostat_menu_control((int)data);
            break;
        default:
            break;
        }
    }
}

void bsp_button_init(void)
{
    button_config_t btn_cfg_right = {
        .type = BUTTON_TYPE_ADC, // 增加一个 ADC 按键
        .adc_button_config = {
            .adc_channel = 4,
            .button_index = BUTTON_SINGLE_CLICK_RIGHT,
            .min = 300,  // 0.3 V
            .max = 500,  // 0.5 V
        }
    };
    button_handle_t btn = iot_button_create(&btn_cfg_right);
    assert(btn);
    esp_err_t err = iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, (void *)BUTTON_SINGLE_CLICK_RIGHT);
    ESP_ERROR_CHECK(err);

    button_config_t btn_cfg_center = {
        .type = BUTTON_TYPE_ADC, // 增加一个 ADC 按键
        .adc_button_config = {
            .adc_channel = 4,
            .button_index = BUTTON_SINGLE_CLICK_CENTER,
            .min = 1000,  // 1 V
            .max = 1200,  // 1.2 V
        }
    };
    btn = iot_button_create(&btn_cfg_center);
    assert(btn);
    err = iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, (void *)BUTTON_SINGLE_CLICK_CENTER);
    ESP_ERROR_CHECK(err);
    err = iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb, (void *)BUTTON_DOUBLE_CLICK_CENTER_LONG_PRESS);
    ESP_ERROR_CHECK(err);

    button_config_t btn_cfg_left = {
        .type = BUTTON_TYPE_ADC, // 增加一个 ADC 按键
        .adc_button_config = {
            .adc_channel = 4,
            .button_index = BUTTON_SINGLE_CLICK_LEFT,
            .min = 2300,  // 2.3 V
            .max = 2500,  // 2.5 V
        }
    };
    btn = iot_button_create(&btn_cfg_left);
    assert(btn);
    err = iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, (void *)BUTTON_SINGLE_CLICK_LEFT);
    ESP_ERROR_CHECK(err);
}