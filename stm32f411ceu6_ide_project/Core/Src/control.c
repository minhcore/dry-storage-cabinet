#include "control.h"
#include <math.h>

void control_init(
		control_t* control, TIM_HandleTypeDef* tim, uint32_t tim_channel,
		GPIO_TypeDef* peltier_port, uint16_t peltier_pin,
		GPIO_TypeDef* hot_fan_port, uint16_t hot_fan_pin,
		GPIO_TypeDef* error_led_port, uint16_t error_led_pin,
		GPIO_TypeDef* normal_led_port, uint16_t normal_led_pin,
		float target_hum)
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
	control->ema_alpha = 0.5;
	control->k_factor_on = 10.0;
	control->k_factor_off = 5.0;

	// start pwm of cold fan
	HAL_TIM_PWM_Start(tim, tim_channel);

}

static void control_peltier_hot_fan(control_t* control)
{
	HAL_GPIO_WritePin(control->peltier_port, control->peltier_pin, control->peltier_hot_fan_on & 0x01);
	HAL_GPIO_WritePin(control->hot_fan_port, control->hot_fan_pin, control->peltier_hot_fan_on & 0x01);}

static void control_cold_fan(control_t* control, uint8_t pwm)
{
	__HAL_TIM_SET_COMPARE(control->tim, control->tim_channel, (pwm * COLD_FAN_ARR) / 100);
}

static float dew_point_calculate(control_t* control, sht30_t* sht30)
{
	// Using Magnus-Tetens formula
	float alpha = 17.26 * sht30->temp / (237.3 + sht30->temp) + logf(sht30->hum/100);
	return (237.3 * alpha / (17.27 - alpha));
}

void control_update(control_t *control, ntc_t* ntc, sht30_t* sht30)
{
	float v = control->filtered_hum - control->prev_hum;
	control->prev_hum = control->filtered_hum;

	float turn_on_thres = (control->target_hum + 0.5) - (control->k_factor_on * v);
	float turn_off_thres = (control->target_hum - 0.5) - (control->k_factor_off * v);

	if (control->filtered_hum >= turn_on_thres)
	{
		control->peltier_hot_fan_on = true;
		HAL_GPIO_WritePin(control->error_led_port, control->error_led_pin, 1);
	}
	else if (control->filtered_hum <= turn_off_thres)
	{
		control->peltier_hot_fan_on = false;
		HAL_GPIO_WritePin(control->error_led_port, control->error_led_pin, 0);
	}

	control_peltier_hot_fan(control);

	float dew_point_temp = dew_point_calculate(control, sht30);
	if (ntc->temp <= (dew_point_temp - 3) && (control->peltier_hot_fan_on == true))
	{
		control_cold_fan(control, 50);
		HAL_GPIO_WritePin(control->normal_led_port, control->normal_led_pin, 1);
	}
	else if (ntc->temp > (dew_point_temp - 1) || (control->peltier_hot_fan_on == false))
	{
		control_cold_fan(control, 0);
		HAL_GPIO_WritePin(control->normal_led_port, control->normal_led_pin, 0);
	}

}

void control_filter_hum(control_t *control, sht30_t* sht30)
{
	control->filtered_hum = (control->ema_alpha * sht30->hum) + ((1 - control->ema_alpha) * control->filtered_hum);
}
