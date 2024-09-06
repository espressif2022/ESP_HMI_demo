#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR_DRIVER_INPUT_A          GPIO_NUM_6            // Input terminal A of motor driver
#define MOTOR_DRIVER_INPUT_B          GPIO_NUM_7            // Input terminal B of motor driver
#define MOTOR_LEDC_TIMER              LEDC_TIMER_1          // Timer used for motor driver PWM
#define MOTOR_LEDC_MODE               LEDC_LOW_SPEED_MODE   // Set LEDC mode to low speed mode
#define MOTOR_LEDC_CHANNEL_A          LEDC_CHANNEL_1        // Set LEDC channel A for motor driver PWM
#define MOTOR_LEDC_CHANNEL_B          LEDC_CHANNEL_2        // Set LEDC channel B for motor driver PWM
#define MOTOR_LEDC_DUTY_RES           LEDC_TIMER_10_BIT     // Set duty resolution to 10 bits
#define MOTOR_LEDC_DUTY_MAX           (1023)                // Set maximum duty cycle to 1023
#define MOTOR_LEDC_FREQUENCY          (40000)               // Set LEDC frequency to 40 kHz


/**
 * @brief   Motor control state
 * This enum defines the control state of the motor.
 * The motor can be in waiting, forward, backward, or stop state. 
 */
typedef enum {
    MOTOR_WAITING = 0,  // Waiting for the motor to be started
    MOTOR_FORWARD,      // Forward vibration
    MOTOR_BACKWARD,     // Backward vibration
    MOTOR_STOP,         // Stop vibration
} motor_ctrl_state_t;

/**
 * @brief   Motor state
 * This enum defines the state of the motor.
 * The motor can be in running or paused state. 
 * The running state means the motor is vibrating, and the paused state means the motor is not vibrating. 
 */
typedef enum {
    MOTOR_RUNNING = 0,  // Running
    MOTOR_PAUSED = 1,   // Paused
} motor_state_t;

/**
 * @brief   Motor frequency level
 * This enum defines the frequency level of the motor.
 * The higher the frequency level, the higher the frequency of the motor vibration. 
 */
typedef enum {
    MOTOR_FREQUENCY_LEVEL_1 = 1,
    MOTOR_FREQUENCY_LEVEL_2,
    MOTOR_FREQUENCY_LEVEL_3,
    MOTOR_FREQUENCY_LEVEL_4,
} motor_frequency_level_t;

/**
 * @brief   Motor amplitude level
 * This enum defines the amplitude level of the motor.
 * The higher the amplitude level, the higher the amplitude of the motor vibration. 
 */
typedef enum {
    MOTOR_AMPLITUDE_LEVEL_1 = 1,
    MOTOR_AMPLITUDE_LEVEL_2,
    MOTOR_AMPLITUDE_LEVEL_3,
    MOTOR_AMPLITUDE_LEVEL_4,
} motor_amplitude_level_t;

/**
 * @brief   Initialize the motor driver 
 * @param   motor_pin_a input terminal A of motor driver
 * @param   motor_pin_b input terminal B of motor driver
 * @return  ESP_OK on success, 
 *          ESP_FAIL on failure
 */
esp_err_t motor_driver_init(gpio_num_t motor_pin_a, gpio_num_t motor_pin_b);

/**
 * @brief   motor start function
 * @note    This function is used to start the motor vibration task. 
 *          It will create the motor vibration task if it is not running, and set the motor state to running.   
 */ 
void motor_start(void);

/**
 * @brief   motor stop function
 * @note    This function is used to stop the motor vibration task. 
 *          It will delete the motor vibration task if it is running, and set the motor state to paused.   
 */ 
void motor_stop(void);

/**
 * @brief   set motor parameters
 * @param   frequency_level frequency level (1-4)
 * @param   amplitude_level amplitude level (1-4)
 * @note    This function is used to set the motor parameters. 
 *          It will set the motor frequency and amplitude based on the given frequency level and amplitude level.   
 *          The higher the frequency level, the higher the frequency of the motor vibration.   
 *          The higher the amplitude level, the higher the amplitude of the motor vibration.    
 */
void motor_set_params(motor_frequency_level_t frequency_level, motor_amplitude_level_t amplitude_level);

/**
 * @brief   get motor state
 * @return  motor state (running or paused)
 * @note    This function is used to get the state of the motor. 
 */
uint8_t motor_get_state(void);

#ifdef __cplusplus
}
#endif
