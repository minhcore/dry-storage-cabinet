#ifndef CONTROL_H_
#define CONTROL_H_

#include <stdbool.h>
#include "stm32f4xx.h"
#include "ntc.h"
#include "sht30.h"

#define COLD_FAN_ARR               999U
#define EMA_ALPHA                  0.5f

#define MIN_ON_TIME_MS             25000U
#define MIN_OFF_TIME_MS            20000U

#define OVERSHOOT_LEARN_ALPHA      0.15f
#define OVERSHOOT_MIN_MARGIN       0.3f
#define OVERSHOOT_MAX_STEP         0.15f

#define REVERSAL_CONFIRM_RH        0.15f

typedef struct
{
    /* Cold fan PWM */
    TIM_HandleTypeDef *tim;
    uint32_t tim_channel;

    /* Peltier and hot-side fan */
    GPIO_TypeDef *peltier_port;
    GPIO_TypeDef *hot_fan_port;
    uint16_t peltier_pin;
    uint16_t hot_fan_pin;
    bool peltier_hot_fan_on;

    /* Status LEDs */
    GPIO_TypeDef *error_led_port;
    GPIO_TypeDef *normal_led_port;
    uint16_t error_led_pin;
    uint16_t normal_led_pin;

    /* Control values */
    float target_hum;
    float filtered_hum;

    /* UART/live-plot debug values */
    float debug_turn_on;
    float debug_turn_off;
    float debug_dewpoint;
    uint8_t debug_peltier;
    uint8_t debug_cold_fan;

    /* Overshoot learning */
    float overshoot_off_learned;
    float overshoot_on_learned;
    float cycle_extreme_hum;
    bool learning_started;
    bool phase_reversed;
    uint32_t last_cycle_duration_ms;

    /* Phase diagnostics */
    float reversal_delta;
    bool cold_fan_used_this_phase;
    uint32_t state_elapsed_ms;

} control_t;

void control_init(
        control_t *control,
        TIM_HandleTypeDef *tim,
        uint32_t tim_channel,
        GPIO_TypeDef *peltier_port,
        uint16_t peltier_pin,
        GPIO_TypeDef *hot_fan_port,
        uint16_t hot_fan_pin,
        GPIO_TypeDef *error_led_port,
        uint16_t error_led_pin,
        GPIO_TypeDef *normal_led_port,
        uint16_t normal_led_pin,
        float target_hum);

void control_update(
        control_t *control,
        ntc_t *ntc,
        sht30_t *sht30);

void control_filter_hum(
        control_t *control,
        sht30_t *sht30);

#endif /* CONTROL_H_ */
