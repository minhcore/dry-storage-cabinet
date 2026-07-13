#ifndef CONTROL_H_
#define CONTROL_H_

#include <stdbool.h>
#include "stm32f4xx.h"
#include "ntc.h"
#include "sht30.h"

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
	float filtered_hum;
	float ema_alpha;
	float prev_hum;
	float k_factor_on;
	float k_factor_off;
	float dew_point_temp;

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
