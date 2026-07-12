#include "control.h"

void control_init(
		control_t* control, TIM_HandleTypeDef* tim, uint32_t tim_channel,
		GPIO_TypeDef* peltier_port, uint16_t peltier_pin,
		GPIO_TypeDef* hot_fan_port, uint16_t hot_fan_pin,
		GPIO_TypeDef* error_led_port, uint16_t error_led_pin,
		GPIO_TypeDef* normal_led_port, uint16_t normal_led_pin,
		float target_hum, float hum_off_offset, float hum_on_offset,
		float cold_fan_on_temp, float cold_fan_off_temp)
{
	control->tim = tim;
	control->tim_channel = tim_channel;
	control->peltier_port = peltier_port;
	control->peltier_pin = peltier_pin;
	control->hot_fan_port = hot_fan_port;
	control->hot_fan_pin = hot_fan_pin;
	control->error_led_port = error_led_port;
	control->error_led_pin = error_led_pin;
	control->normal_led_port = normal_led_port;
	control->normal_led_pin = normal_led_pin;

	// set default value
	control->target_hum = target_hum;
	control->hum_off_offset = hum_off_offset;
	control->hum_on_offset = hum_on_offset;
	control->cold_fan_on_temp = cold_fan_on_temp;
	control->cold_fan_off_temp = cold_fan_off_temp;

	// start pwm of cold fan
	HAL_TIM_PWM_Start(tim, tim_channel);

	control->stable = false;
}

static void control_peltier_hot_fan(control_t* control)
{
	HAL_GPIO_WritePin(control->peltier_port, control->peltier_pin, control->peltier_hot_fan_on & 0x01);
	HAL_GPIO_WritePin(control->hot_fan_port, control->hot_fan_pin, control->peltier_hot_fan_on & 0x01);}

static void control_cold_fan(control_t* control, uint8_t pwm)
{
	__HAL_TIM_SET_COMPARE(control->tim, control->tim_channel, (pwm * COLD_FAN_ARR) / 100);
}

void control_update(control_t *control, ntc_t* ntc, float cur_hum)
{
	if (cur_hum >= (control->target_hum + control->hum_on_offset))
	{
		control->peltier_hot_fan_on = true;
		HAL_GPIO_WritePin(control->error_led_port, control->error_led_pin, 1);
	}
	else if (cur_hum <= (control->target_hum - control->hum_off_offset))
	{
		control->peltier_hot_fan_on = false;
		HAL_GPIO_WritePin(control->error_led_port, control->error_led_pin, 0);
	}

	control_peltier_hot_fan(control);

	if (ntc->temp < control->cold_fan_on_temp)
	{
		control_cold_fan(control, 50);
		HAL_GPIO_WritePin(control->normal_led_port, control->normal_led_pin, 1);
	}
	else if (ntc->temp > control->cold_fan_off_temp)
	{
		control_cold_fan(control, 0);
		HAL_GPIO_WritePin(control->normal_led_port, control->normal_led_pin, 0);
	}

	if (!control->stable)
	{
		if (cur_hum <= control->target_hum)
		{
			control->stable = true;
			control->max_hum = cur_hum;
			control->min_hum = cur_hum;
		}
	}
	else
	{
		if (cur_hum < control->min_hum)
			control->min_hum = cur_hum;

		if (cur_hum > control->max_hum)
			control->max_hum = cur_hum;
	}
}
