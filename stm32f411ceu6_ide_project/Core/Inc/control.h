#ifndef CONTROL_H_
#define CONTROL_H_

#include <stdbool.h>
#include "stm32f4xx.h"
#include "ntc.h"
#include "sht30.h"

#define COLD_FAN_ARR 		999
#define EMA_ALPHA 			0.67
#define K_FACTOR_ON 		5.0
#define K_FACTOR_OFF 		6.5
#define MIN_ON_TIME_MS 		8000 	// 8 seconds
#define MIN_OFF_TIME_MS 	5000 	// 5 seconds

#define OVERSHOOT_OFF_SEED_BASE   2.0f
#define OVERSHOOT_OFF_SEED_SLOPE  0.1f
#define OVERSHOOT_ON_SEED_BASE    1.5f
#define OVERSHOOT_ON_SEED_SLOPE   0.055f
#define OVERSHOOT_SEED_REF_TARGET 40.0f

#define OVERSHOOT_LEARN_ALPHA     0.15f

#define OVERSHOOT_MIN_MARGIN      0.3f

typedef struct
{
	// cold fan
	TIM_HandleTypeDef *tim;
	uint32_t tim_channel;

	// peltier and hot fan
	GPIO_TypeDef* peltier_port;
	GPIO_TypeDef* hot_fan_port;
	uint16_t peltier_pin;
	uint16_t hot_fan_pin;
	bool peltier_hot_fan_on;

	// status led
	GPIO_TypeDef* error_led_port;
	GPIO_TypeDef* normal_led_port;
	uint16_t error_led_pin;
	uint16_t normal_led_pin;

	// value for control
	float target_hum;
	float filtered_hum;
	float prev_hum;
	float dew_point_temp;

	// debug
	float debug_v, debug_turn_on, debug_turn_off, debug_dewpoint;
	uint32_t debug_current_tick, debug_prev_tick;
	uint8_t debug_peltier, debug_cold_fan;

	// new
	float overshoot_off_learned;
	float overshoot_on_learned;
	float cycle_extreme_hum;

} control_t;

void control_init(control_t* control, TIM_HandleTypeDef* tim, uint32_t tim_channel,
				GPIO_TypeDef* peltier_port, uint16_t peltier_pin,
				GPIO_TypeDef* hot_fan_port, uint16_t hot_fan_pin,
				GPIO_TypeDef* error_led_port, uint16_t error_led_pin,
				GPIO_TypeDef* normal_led_port, uint16_t normal_led_pin,
				float target_hum);
void control_update(control_t *control, ntc_t* ntc, sht30_t* sht30);
void control_filter_hum(control_t *control, sht30_t* sht30);

#endif
