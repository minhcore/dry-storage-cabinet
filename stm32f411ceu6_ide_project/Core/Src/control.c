#include "control.h"
#include <math.h>

static uint32_t hum_delay_tick = 0;
static uint32_t buzzer_tick = 0;
static float prev_target = DEFAULT_TARGET_HUM;

static control_profile_t control_get_profile(float target_hum)
{
    control_profile_t profile;

    if (target_hum <= 40.0)
    {
        profile.on_advance = LOW_ON_ADVANCE;
        profile.off_advance = LOW_OFF_ADVANCE;
        profile.min_on_ms = LOW_MIN_ON_MS;
        profile.min_off_ms = LOW_MIN_OFF_MS;
        profile.id = 0;
    }
    else if (target_hum <= 50.0)
    {
        profile.on_advance = MID_ON_ADVANCE;
        profile.off_advance = MID_OFF_ADVANCE;
        profile.min_on_ms = MID_MIN_ON_MS;
        profile.min_off_ms = MID_MIN_OFF_MS;
        profile.id = 1;
    }
    else
    {
        profile.on_advance = HIGH_ON_ADVANCE;
        profile.off_advance = HIGH_OFF_ADVANCE;
        profile.min_on_ms = HIGH_MIN_ON_MS;
        profile.min_off_ms = HIGH_MIN_OFF_MS;
        profile.id = 2;
    }

    return profile;
}

static void control_peltier_output(control_t *control)
{
	HAL_GPIO_WritePin(control->peltier_port,control->peltier_pin,control->peltier_on? 1 : 0);
}

static void control_cold_fan_output(control_t *control, uint8_t pwm)
{
    __HAL_TIM_SET_COMPARE(control->tim, control->tim_channel, (pwm * COLD_FAN_ARR) / 100);
}

static void control_set_peltier(control_t *control, bool on, uint32_t current_tick)
{
    if (control->peltier_on == on)
    {
        return;
    }

    control->peltier_on = on;
    control->last_state_tick = current_tick;

    HAL_GPIO_WritePin(control->peltier_led_port, control->peltier_led_pin, on ? 1 : 0);

    control_peltier_output(control);
}

static void control_set_cold_fan(control_t *control, bool on)
{
    control->cold_fan_on = on;

    control_cold_fan_output(control, on ? 50 : 0);

}

static float dew_point_calculate(const sht30_t *sht30)
{
    // Magnus-Tetens approximation
    float a = 17.27f;
    float b = 237.3f;

    float rh = sht30->hum;

    float gamma =(a * sht30->temp) / (b + sht30->temp) + logf(rh / 100.0);

    return (b * gamma) / (a - gamma);
}

void control_init(
        control_t *control,
        TIM_HandleTypeDef *tim,
        uint32_t tim_channel,
        GPIO_TypeDef *peltier_port,
        uint16_t peltier_pin,
        GPIO_TypeDef *hot_fan_port,
        uint16_t hot_fan_pin,
        GPIO_TypeDef *peltier_led_port,
        uint16_t peltier_led_pin,
		GPIO_TypeDef* buzzer_port,
		uint16_t buzzer_pin)
{
    control->tim = tim;
    control->tim_channel = tim_channel;

    control->peltier_port = peltier_port;
    control->peltier_pin = peltier_pin;

    control->hot_fan_port = hot_fan_port;
    control->hot_fan_pin = hot_fan_pin;

    control->peltier_led_port = peltier_led_port;
    control->peltier_led_pin = peltier_led_pin;

    control->buzzer_port = buzzer_port;
    control->buzzer_pin = buzzer_pin;

    control->target_hum = DEFAULT_TARGET_HUM;

    control->peltier_on = false;
    control->cold_fan_on = false;
    control->control_started = false;

    control->temp_limit = DEFAULT_TEMP_LIMIT_ALARM;
    control->hum_margin = DEFAULT_HUM_MARGIN_ALARM;
    control->hum_delay_mins = DEFAULT_HUM_DELAY_ALARM;
    control->is_buzzer = true;
    control->is_alarm_temp = false;
    control->is_alarm_hum = false;

    control->last_state_tick = HAL_GetTick();
    control->state_elapsed_ms = 0;

    control_profile_t profile = control_get_profile(DEFAULT_TARGET_HUM);

    control->debug_turn_on = control->target_hum - profile.on_advance;
    control->debug_turn_off = control->target_hum + profile.off_advance;
    control->debug_dewpoint = 0.0;

    control->debug_peltier = 0;
    control->debug_cold_fan = 0;
    control->debug_profile = profile.id;
    control->debug_condensing_started = 0;
    control->debug_fan_boost_allowed = 0;

    HAL_TIM_PWM_Start(tim, tim_channel);

    control_peltier_output(control);
    control_set_cold_fan(control, false);

    // Turn on Hot Fan forever
    HAL_GPIO_WritePin(control->hot_fan_port, control->hot_fan_pin, 1);
}

void control_update(
        control_t *control,
        ntc_t *ntc,
        sht30_t *sht30)
{
	// Peltier Logic
    uint32_t current_tick = HAL_GetTick();

    control_profile_t profile = control_get_profile(control->target_hum);

    float raw_hum = sht30->hum;

    float turn_on_thres = control->target_hum - profile.on_advance;

    float turn_off_thres = control->target_hum + profile.off_advance;

    float dew_point_temp = dew_point_calculate(sht30);

    float thermal_margin = dew_point_temp - ntc->temp;

    bool condensing_started = thermal_margin >= CONDENSING_MARGIN;

    uint32_t state_elapsed = current_tick - control->last_state_tick;

    bool first_update = !control->control_started;

    control->control_started = true;

    if (!control->peltier_on)
    {
        bool minimum_off_passed = state_elapsed >= profile.min_off_ms;

        /* Allow immediate startup from a humid initial condition. */
        bool may_turn_on = first_update || minimum_off_passed;

        if (may_turn_on && (raw_hum >= turn_on_thres))
        {
            control_set_peltier(control, true, current_tick);

            state_elapsed = 0;
        }
    }
    else
    {
        bool minimum_on_passed = state_elapsed >= profile.min_on_ms;

        if (minimum_on_passed && condensing_started && (raw_hum <= turn_off_thres))
        {
            control_set_peltier(control, false, current_tick);

            state_elapsed = 0;
        }
    }

    control_peltier_output(control);

    state_elapsed = current_tick - control->last_state_tick;

    // Cold Fan Logic
    bool thermal_ready = thermal_margin >= COLD_FAN_ON_TEMP_MARGIN;

    bool thermal_not_ready = thermal_margin < COLD_FAN_OFF_TEMP_MARGIN;

    bool humidity_demand = raw_hum >= (control->target_hum + COLD_FAN_ON_HUM_MARGIN);

    bool humidity_not_demand = raw_hum <= (control->target_hum + COLD_FAN_OFF_HUM_MARGIN);

    bool far_above_target = raw_hum >= (control->target_hum + COLD_FAN_FAR_ABOVE_OFFSET);

    bool boost_delay_passed = control->peltier_on && (state_elapsed >= COLD_FAN_BOOST_DELAY_MS);

    bool fan_boost_allowed = far_above_target || boost_delay_passed;

    if (!control->cold_fan_on)
    {
        if (control->peltier_on && thermal_ready && humidity_demand && fan_boost_allowed)
        {
            control_set_cold_fan(control, true);
        }
    }
    else
    {
        if (!control->peltier_on || thermal_not_ready || humidity_not_demand)
        {
            control_set_cold_fan(control, false);
        }
    }

    if (control->is_alarm_hum || control->is_alarm_temp)
    {
    	if(control->is_buzzer)
    	{
    		if ((HAL_GetTick() - buzzer_tick) >= 1000)
    		    {
    		    	HAL_GPIO_TogglePin(control->buzzer_port, control->buzzer_pin);
    		    	buzzer_tick = HAL_GetTick();
    		    }
    	}
    }

    control->state_elapsed_ms = state_elapsed;

    control->debug_turn_on = turn_on_thres;
    control->debug_turn_off = turn_off_thres;
    control->debug_dewpoint = dew_point_temp;

    control->debug_peltier =
            control->peltier_on ? 1U : 0U;

    control->debug_cold_fan =
            control->cold_fan_on ? 1U : 0U;

    control->debug_profile = profile.id;

    control->debug_condensing_started =
            condensing_started ? 1U : 0U;

    control->debug_fan_boost_allowed =
            fan_boost_allowed ? 1U : 0U;
}

void control_alarm_hum_check(control_t *control, sht30_t *sht30)
{
	bool target_changed;
	if (control->target_hum != prev_target)
	{
		target_changed = true;
		prev_target = control->target_hum;
	}
	else
	{
		target_changed = false;
	}

	if (target_changed) hum_delay_tick = HAL_GetTick(); // Reset
	else if ((HAL_GetTick() - hum_delay_tick) >= (control->hum_delay_mins * 60 * 1000))
	{
		float pos_margin = control->target_hum + control->hum_margin;
		float neg_margin = control->target_hum - control->hum_margin;
		if ((sht30->hum > pos_margin) || (sht30->hum < neg_margin))
		{
			control->is_alarm_hum = true;
		}
	}
}

void control_alarm_temp_check(control_t* control, sht30_t *sht30)
{
	if (sht30->temp > control->temp_limit) control->is_alarm_temp = true;
}

