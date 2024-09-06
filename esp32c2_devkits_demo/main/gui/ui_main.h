#ifndef _UI_MAIN_H
#define _UI_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "iot_button.h"

/**< GPIO for Button */
#define BOOT_BUTTON_NUM GPIO_NUM_9

/**< display sleep time */
#define DISPLAY_SLEEP_TIME 8u

/**< Screen num and index */
#define SCREEN_NUM 6
typedef enum {
    SYSTEM_START_SCREEN_INDEX = 0,
    HOME_SCREEN_INDEX = 1,
    BRUSHING_SCREEN_INDEX = 2,
    EVALUATION_SCREEN_INDEX = 3,
    CHARGING_SCREEN_INDEX = 4,
    SYSTEM_SLEEP_SCREEN_INDEX = 5,
} screen_index_t;

/**< some data_type */
typedef enum {
    Sun = 0,
    Mon,
    Tue,
    Wed,
    Thu,
    Fri,
    Sat
} weekday_t;

typedef uint8_t battery_power_t;

/**< Total brush time */
typedef int32_t brush_second_t;
typedef int32_t brush_minute_t;
typedef struct {
    brush_second_t brush_second;
    brush_minute_t brush_minute;
} brush_time_t;

/**< Evaluation_Screen automatic switching screen  */
#define SCREEN_AUTO_SWITCH_TIME 5
typedef enum {
    BRUSH_START = 0,
    BRUSH_STOP
} brush_status_t;

typedef enum {
    FIRST_STAGE = 0,
    SECOND_STAGE,
    THIRD_STAGE,
    FOURTH_STAGE,
    FIFTH_STAGE
} brush_effect_t;

/**< Brushing Intensity */
#define BRUSH_STRENTH_MAX 4
typedef enum {
    STRENGTH_LEVEL_0 = 0,
    STRENGTH_LEVEL_1,
    STRENGTH_LEVEL_2,
    STRENGTH_LEVEL_3,
    STRENGTH_LEVEL_4
} brush_strength_t;

typedef enum {
    BUTTON_NULL_PRESS = 0,
    BUTTON_SHORT_PRESS,
    BUTTON_LONG_PRESS
} button_trigger_mode;

typedef struct _button_state_t {
    bool is_trigger;
    button_trigger_mode trigger_mode;
} button_state_t;

extern TimerHandle_t g_brush_timer;
extern brush_second_t g_brush_remain_time;

/**< Evaluation_Screen automatic switching screen  */
extern TimerHandle_t g_auto_switch_timer;
extern uint8_t g_screen_wait_time;

/**< esp memory printf */
void ui_printf_stack();
/**< Toothbrush Ui Initialization,This function must be placed before button_init() */
void ui_toothbrush_screen_init(void);
/**< Change Screen */
void ui_change_to_screen(screen_index_t new_screen_index);
/**< Get current Screen Index */
screen_index_t ui_get_current_screen_index(void);
/**< wifi connect status */
void ui_wifi_status_display(bool state);
/**< Bluetooth connect status */
void ui_bluetooth_status_display(bool state);
/**< Time and Date Week_day Display */
void ui_time_date_display(const char* hour, const char* min, const char* date, weekday_t day);
/**< Battery power display */
void ui_battery_power_display(battery_power_t val);
/**< Countdown to brush-tooth time */
void ui_brush_time_remain_display(brush_second_t val);
void set_brush_total_time(brush_second_t time);
brush_second_t get_brush_total_time(void);
/**< Brushtooth status changed */
void ui_brush_status_change(brush_status_t state);
/**< Brushtooth strength initialization */
void brush_strength_init(void);
/**< Brushtooth strength set */
void brush_strength_set(brush_strength_t strength);
/**< Brushtooth strength get */
brush_strength_t brush_strength_get(void);
/**< Brushtooth strength shifted from 1 --- 4 */
void ui_brush_strength_shift(void);
/**< Clear Brush_Screen and Change to Evaluation_Screen */
void ui_brushing_screen_clear(void);
/**< Get user toothbrush time should be called in Evaluation Screen */
brush_second_t ui_get_brush_time(void);
/**< Clear Evaluation Screen and Change to Home_Screen */
void ui_evaluation_screen_clear(void);
/**< Brushtooth effect evaluate */
void ui_brush_evaluation(brush_effect_t stage);
/**< Charging power display */
void ui_charging_power_display(battery_power_t val);
/**< Set/Reset button press state */
void ui_set_btn_state(button_trigger_mode mode);
void ui_reset_btn_state(void);
/**< Get button state */
bool ui_get_btn_is_trigger(void);
button_trigger_mode ui_get_btn_trigger_mode(void);

/**< UI closed in SYSTEM_SLEEP_SCREEN */
void ui_closed(void);
/**< UI Restart when BUTTON_LONG_PRESS */
void ui_restart(void);

/**< UI display off */
void ui_display_off(screen_index_t screen_index);
/**< UI display on */
void ui_display_on(screen_index_t screen_index);
/**< Display off countdown */
void reset_sleep_countdown_time(void);
void decrease_sleep_countdown_time(void);
bool get_sleep_status(void);
uint8_t get_sleep_countdown_time(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
