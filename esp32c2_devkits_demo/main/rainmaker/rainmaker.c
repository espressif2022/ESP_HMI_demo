#include <string.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_common_events.h>
#include <app_wifi.h>
#include <time.h>
#include "nvs_flash.h"


#define RAINMAKER_NAME       "esp-toothbrush"
#define RAINMAKER_POP        "12345678"
#define DEFAULT_SWITCH       false

static const char *TAG = "rainmaker";
static const char *valid_strs[] = {"Gentle Mode","Standard Mode","Strong Mode","Deep Clean Mode"};
static bool g_rainmaker_connected_status = false;         // rainmaker connected status
static SemaphoreHandle_t g_rainmaker_sem_handle = NULL;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base != RMAKER_COMMON_EVENT) {
        return;
    }
    switch (event_id) {
    case RMAKER_MQTT_EVENT_CONNECTED:
        g_rainmaker_connected_status = true;
        ESP_LOGI(TAG, "RMAKER connected");
        xSemaphoreGive(g_rainmaker_sem_handle);
        break;
    case RMAKER_MQTT_EVENT_DISCONNECTED:
        g_rainmaker_connected_status = false;
        ESP_LOGI(TAG, "RMAKER disconnected");
        break;
    default:
        break;
    }
}

esp_rmaker_device_t* rainmaker_init(esp_rmaker_device_write_cb_t write_cb)
{
    /* Initialize the Wi-Fi, noting that this function should be called first before calling esp_rmaker_init() */
    app_wifi_init();

    /* Initializing the ESP RainMaker Agent
     * Should first call app_wifi_init(), followed by this function, and finally app_wifi_start()
     */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = true,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "esp-toothbrush", "toothbrush");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Creating a Device */
    esp_rmaker_device_t *g_rainmaker_device = esp_rmaker_device_create("esp-toothbrush", ESP_RMAKER_DEVICE_OTHER, NULL);
    /* Adding Callback Functions */
    esp_rmaker_device_add_cb(g_rainmaker_device, write_cb, NULL);
    /* Create standard name param */
    esp_rmaker_device_add_param(g_rainmaker_device, esp_rmaker_name_param_create("name", "esp-toothbrush"));

    esp_rmaker_param_t *power_param = esp_rmaker_param_create("Battery Level", NULL, esp_rmaker_str("100 %"), PROP_FLAG_READ);
    esp_rmaker_param_add_ui_type(power_param, ESP_RMAKER_UI_TEXT);
    esp_rmaker_device_add_param(g_rainmaker_device, power_param);
    esp_rmaker_device_assign_primary_param(g_rainmaker_device, power_param);

    esp_rmaker_param_t *vibration_param = esp_rmaker_param_create("Brushing Mode", NULL, esp_rmaker_str("Gentle Mode"), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_valid_str_list(vibration_param, valid_strs, 4);
    esp_rmaker_param_add_ui_type(vibration_param, ESP_RMAKER_UI_DROPDOWN);
    esp_rmaker_device_add_param(g_rainmaker_device, vibration_param);

    esp_rmaker_param_t *battery_param = esp_rmaker_param_create("state of charge", ESP_RMAKER_PARAM_POWER, esp_rmaker_bool(false), PROP_FLAG_READ);
    esp_rmaker_param_add_ui_type(battery_param, ESP_RMAKER_UI_TOGGLE);
    esp_rmaker_device_add_param(g_rainmaker_device, battery_param);

    esp_rmaker_param_t *brush_time = esp_rmaker_param_create("Set brush time(s)", NULL, esp_rmaker_int(180), PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(power_param, ESP_RMAKER_UI_TEXT);
    esp_rmaker_device_add_param(g_rainmaker_device, brush_time);

    esp_rmaker_param_t *medication_record_one = esp_rmaker_param_create("Brushing time(s)", NULL, esp_rmaker_float(0.0), PROP_FLAG_READ | PROP_FLAG_TIME_SERIES);
    esp_rmaker_device_add_param(g_rainmaker_device, medication_record_one);

    /* Add current device to node */
    esp_rmaker_node_add_device(node, g_rainmaker_device);

    /* Register an event handler to the system event loop.
     * This event handler is used to determine whether Rainmaker has connected successfully or not.
     */
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    /* Enabling the ESP RainMaker Agent */
    esp_rmaker_start();

    /* Start Wi-Fi */
    esp_err_t err = app_wifi_start(RAINMAKER_NAME,RAINMAKER_POP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        nvs_flash_erase();
        abort();
    }
	ESP_LOGI(TAG, "app_wifi_start finish\r\n");
    return g_rainmaker_device;
}

int8_t rainmaker_get_brushing_mode_num(char *brushing_mode)
{
    for (int8_t i = 0; i < 4; i++) {
        if (strcmp(brushing_mode, valid_strs[i]) == 0) {
            ESP_LOGI(TAG,"Brushing Mode %d ",i+1);
            return i+1;
        }
    }
    ESP_LOGI(TAG,"brushing_mode err");
    return -1;
}

const char* rainmaker_get_brushing_mode_str(int brushing_mode)
{
    ESP_LOGI(TAG,"Brushing Mode %d ",brushing_mode);
    if (brushing_mode < 4 && brushing_mode >=0) {
        ESP_LOGI(TAG,"Brushing Mode %s ",valid_strs[brushing_mode]);
        return valid_strs[brushing_mode];
    }
    ESP_LOGI(TAG,"brushing_mode err");
    return NULL;
}

bool rainmaker_get_connect_status(void)
{
    return g_rainmaker_connected_status;
}

void rainmaker_wait_connect(void)
{
    g_rainmaker_sem_handle = xSemaphoreCreateBinary();
    BaseType_t SemRet = xSemaphoreTake(g_rainmaker_sem_handle,portMAX_DELAY);
    if (SemRet == pdPASS) {
        ESP_LOGI(TAG,"xSemaphoreGive succeed\r\n");
    } else {
        ESP_LOGE(TAG,"xSemaphoreGive failed\r\n");
        esp_restart();
    }
    vTaskDelay(pdMS_TO_TICKS(10*1000));
}
