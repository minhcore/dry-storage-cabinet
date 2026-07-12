#ifndef CONTROL_H_
#define CONTROL_H_

#include <stdbool.h>
#include "stm32f4xx.h"
#include "ntc.h"

#define COLD_FAN_ARR 999

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
	float hum_off_offset;
	float hum_on_offset;
	float cold_fan_on_temp;
	float cold_fan_off_temp;

	float max_hum;
	float min_hum;
	bool stable;
} control_t;

void control_init(control_t* control, TIM_HandleTypeDef* tim, uint32_t tim_channel,
				GPIO_TypeDef* peltier_port, uint16_t peltier_pin,
				GPIO_TypeDef* hot_fan_port, uint16_t hot_fan_pin,
				GPIO_TypeDef* error_led_port, uint16_t error_led_pin,
				GPIO_TypeDef* normal_led_port, uint16_t normal_led_pin,
				float target_hum, float hum_off_offset, float hum_on_offset,
				float cold_fan_on_temp, float cold_fan_off_temp);
void control_update(control_t *control, ntc_t* ntc, float cur_hum);

#endif
