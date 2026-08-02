#include "control.h"
#include <math.h>

static uint32_t current_tick, prev_tick, last_state_tick;

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

	// new
	control->overshoot_off_learned = OVERSHOOT_OFF_SEED_BASE + OVERSHOOT_OFF_SEED_SLOPE * (target_hum - OVERSHOOT_SEED_REF_TARGET);
	control->overshoot_on_learned = OVERSHOOT_ON_SEED_BASE + OVERSHOOT_ON_SEED_SLOPE * (target_hum - OVERSHOOT_SEED_REF_TARGET);
	control->cycle_extreme_hum = control->filtered_hum;

	// start pwm of cold fan
	HAL_TIM_PWM_Start(tim, tim_channel);

	// update current_tick and prev_tick using for derivatives
	current_tick = HAL_GetTick();
	prev_tick = HAL_GetTick();
	last_state_tick = HAL_GetTick();

	// turn on Hot Fan
	HAL_GPIO_WritePin(control->hot_fan_port, control->hot_fan_pin, 1);

}

static void control_peltier_hot_fan(control_t* control)
{
	HAL_GPIO_WritePin(control->peltier_port, control->peltier_pin, control->peltier_hot_fan_on & 0x01);
	//HAL_GPIO_WritePin(control->hot_fan_port, control->hot_fan_pin, control->peltier_hot_fan_on & 0x01);
}
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
	current_tick = HAL_GetTick();

	// Calculate derivatives value
	float derivatives_v = (control->filtered_hum - control->prev_hum) * 1000.0 / (current_tick - prev_tick);
	prev_tick = current_tick;
	control->prev_hum = control->filtered_hum;

	// new
	if (control->peltier_hot_fan_on)
	{
		// đang ON: theo dõi ĐỈNH (giá trị lớn nhất) filtered_hum trong chu kỳ này
		if (control->filtered_hum > control->cycle_extreme_hum)
			control->cycle_extreme_hum = control->filtered_hum;
	}
	else
	{
		// đang OFF: theo dõi ĐÁY (giá trị nhỏ nhất) filtered_hum trong chu kỳ này
		if (control->filtered_hum < control->cycle_extreme_hum)
			control->cycle_extreme_hum = control->filtered_hum;
	}

	static bool pending_learn = false;
	static bool learn_for_off_phase = false;

	if (pending_learn)
	{
		if (learn_for_off_phase)
		{
			// vừa kết thúc pha OFF: cycle_extreme_hum là đáy đã chạm
			float actual_off_overshoot = control->target_hum - control->cycle_extreme_hum;
			control->overshoot_off_learned = OVERSHOOT_LEARN_ALPHA * actual_off_overshoot
					+ (1.0f - OVERSHOOT_LEARN_ALPHA) * control->overshoot_off_learned;
			if (control->overshoot_off_learned < OVERSHOOT_MIN_MARGIN)
				control->overshoot_off_learned = OVERSHOOT_MIN_MARGIN;
		}
		else
		{
			// vừa kết thúc pha ON: cycle_extreme_hum là đỉnh đã chạm
			float actual_on_overshoot = control->cycle_extreme_hum - control->target_hum;
			control->overshoot_on_learned = OVERSHOOT_LEARN_ALPHA * actual_on_overshoot
					+ (1.0f - OVERSHOOT_LEARN_ALPHA) * control->overshoot_on_learned;
			if (control->overshoot_on_learned < OVERSHOOT_MIN_MARGIN)
				control->overshoot_on_learned = OVERSHOOT_MIN_MARGIN;
		}
		pending_learn = false;
		control->cycle_extreme_hum = control->filtered_hum; // reset cho chu kỳ mới
	}

	float turn_on_thres  = control->target_hum - control->overshoot_on_learned;
	float turn_off_thres = control->target_hum + control->overshoot_off_learned;
//	// Calculate on/off thresholds
//	float turn_on_thres = control->target_hum - (K_FACTOR_ON * derivatives_v);
//	float turn_off_thres = control->target_hum - (K_FACTOR_OFF * derivatives_v);
//
	// Control the Peltier, Hot Fan and Cold Fan
	if (!control->peltier_hot_fan_on) // System is OFF
	{
		if ((control->filtered_hum >= turn_on_thres) && ((current_tick - last_state_tick) >= MIN_OFF_TIME_MS))
		{
			control->peltier_hot_fan_on = true;
			HAL_GPIO_WritePin(control->error_led_port, control->error_led_pin, 1);
			last_state_tick = current_tick;
			pending_learn = true;
			learn_for_off_phase = true;
		}
	}
	else // System is ON
	{
		if ((control->filtered_hum <= turn_off_thres) && ((current_tick - last_state_tick) >= MIN_ON_TIME_MS))
		{
			control->peltier_hot_fan_on = false;
			HAL_GPIO_WritePin(control->error_led_port, control->error_led_pin, 0);
			last_state_tick = current_tick;
			pending_learn = true;
			learn_for_off_phase = false;
		}
	}

	control_peltier_hot_fan(control);

	// Calculate Dew Point for Cold Fan logic
	float dew_point_temp = dew_point_calculate(control, sht30);
	if (ntc->temp <= (dew_point_temp - 5) && (control->peltier_hot_fan_on == true))
	{
		control_cold_fan(control, 100);
		HAL_GPIO_WritePin(control->normal_led_port, control->normal_led_pin, 1);
		control->debug_cold_fan = 1;
	}
	else if (ntc->temp > (dew_point_temp - 2) || (control->peltier_hot_fan_on == false))
	{
		control_cold_fan(control, 0);
		HAL_GPIO_WritePin(control->normal_led_port, control->normal_led_pin, 0);
		control->debug_cold_fan = 0;
	}

	control->debug_current_tick = current_tick;
	control->debug_prev_tick = prev_tick;
	control->debug_dewpoint = dew_point_temp;
	control->debug_peltier = control->peltier_hot_fan_on;
	control->debug_turn_on = turn_on_thres;
	control->debug_turn_off = turn_off_thres;
	control->debug_v = derivatives_v;
}

void control_filter_hum(control_t *control, sht30_t* sht30)
{
	control->filtered_hum = (EMA_ALPHA * sht30->hum) + ((1 - EMA_ALPHA) * control->filtered_hum);
}
