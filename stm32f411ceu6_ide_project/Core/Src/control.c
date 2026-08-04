#include "control.h"
#include <math.h>

/* Timestamp of the most recent Peltier state transition. */
static uint32_t last_state_tick;

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

    control->target_hum = target_hum;

    control->overshoot_off_learned = OVERSHOOT_MIN_MARGIN;
    control->overshoot_on_learned = OVERSHOOT_MIN_MARGIN;

    control->cycle_extreme_hum = control->filtered_hum;
    control->learning_started = false;

    /*
     * Allow the first startup transition. After that, every transition
     * requires a real peak/trough reversal.
     */
    control->phase_reversed = true;

    control->last_cycle_duration_ms = 0U;
    control->state_elapsed_ms = 0U;
    control->reversal_delta = 0.0f;
    control->cold_fan_used_this_phase = false;

    control->debug_peltier = 0U;
    control->debug_cold_fan = 0U;
    control->debug_turn_on =
            target_hum - control->overshoot_on_learned;
    control->debug_turn_off =
            target_hum + control->overshoot_off_learned;
    control->debug_dewpoint = 0.0f;

    HAL_TIM_PWM_Start(tim, tim_channel);

    last_state_tick = HAL_GetTick();

    /* Hot-side fan is always ON. */
    HAL_GPIO_WritePin(
            control->hot_fan_port,
            control->hot_fan_pin,
            GPIO_PIN_SET);
}

static void control_peltier_output(control_t *control)
{
    HAL_GPIO_WritePin(
            control->peltier_port,
            control->peltier_pin,
            control->peltier_hot_fan_on
                    ? GPIO_PIN_SET
                    : GPIO_PIN_RESET);
}

static void control_cold_fan(control_t *control, uint8_t pwm)
{
    __HAL_TIM_SET_COMPARE(
            control->tim,
            control->tim_channel,
            (pwm * COLD_FAN_ARR) / 100U);
}

static float dew_point_calculate(sht30_t *sht30)
{
    /* Magnus-Tetens formula. */
    float alpha =
            17.26f * sht30->temp / (237.3f + sht30->temp)
            + logf(sht30->hum / 100.0f);

    return 237.3f * alpha / (17.27f - alpha);
}

void control_update(
        control_t *control,
        ntc_t *ntc,
        sht30_t *sht30)
{
    uint32_t current_tick = HAL_GetTick();

    /*
     * Learning is performed one update after a state transition so the
     * completed phase extreme remains available until it has been consumed.
     */
    static bool pending_learn = false;
    static bool learn_for_off_phase = false;

    if (pending_learn)
    {
        if (control->learning_started)
        {
            float *target_margin;
            float actual;

            if (learn_for_off_phase)
            {
                target_margin =
                        &control->overshoot_off_learned;

                actual =
                        control->target_hum
                        - control->cycle_extreme_hum;
            }
            else
            {
                target_margin =
                        &control->overshoot_on_learned;

                actual =
                        control->cycle_extreme_hum
                        - control->target_hum;
            }

            float step =
                    OVERSHOOT_LEARN_ALPHA
                    * (actual - *target_margin);

            if (step > OVERSHOOT_MAX_STEP)
            {
                step = OVERSHOOT_MAX_STEP;
            }

            if (step < -OVERSHOOT_MAX_STEP)
            {
                step = -OVERSHOOT_MAX_STEP;
            }

            *target_margin += step;

            if (*target_margin < OVERSHOOT_MIN_MARGIN)
            {
                *target_margin = OVERSHOOT_MIN_MARGIN;
            }
        }

        control->learning_started = true;
        pending_learn = false;

        /* Start extreme tracking for the new phase. */
        control->cycle_extreme_hum =
                control->filtered_hum;
        control->reversal_delta = 0.0f;
    }

    /*
     * control_init() runs before the first valid SHT30 sample, therefore the
     * first update must initialize cycle_extreme_hum from filtered_hum.
     */
    if (control->cycle_extreme_hum <= 0.0f)
    {
        control->cycle_extreme_hum =
                control->filtered_hum;
    }

    /* Track peak/trough and calculate distance from the current extreme. */
    if (control->peltier_hot_fan_on)
    {
        /* During ON, humidity may initially keep rising: track the peak. */
        if (control->filtered_hum >
                control->cycle_extreme_hum)
        {
            control->cycle_extreme_hum =
                    control->filtered_hum;
        }

        control->reversal_delta =
                control->cycle_extreme_hum
                - control->filtered_hum;
    }
    else
    {
        /* During OFF, humidity may initially keep falling: track the trough. */
        if (control->filtered_hum <
                control->cycle_extreme_hum)
        {
            control->cycle_extreme_hum =
                    control->filtered_hum;
        }

        control->reversal_delta =
                control->filtered_hum
                - control->cycle_extreme_hum;
    }

    if (control->reversal_delta < 0.0f)
    {
        control->reversal_delta = 0.0f;
    }

    if (control->reversal_delta >= REVERSAL_CONFIRM_RH)
    {
        control->phase_reversed = true;
    }

    float turn_on_thres =
            control->target_hum
            - control->overshoot_on_learned;

    float turn_off_thres =
            control->target_hum
            + control->overshoot_off_learned;

    if (!control->peltier_hot_fan_on)
    {
        /*
         * OFF -> ON only after the trough has been confirmed, humidity has
         * risen to the anticipatory ON threshold, and minimum OFF time passed.
         */
        if (control->phase_reversed
                && control->filtered_hum >= turn_on_thres
                && (current_tick - last_state_tick)
                        >= MIN_OFF_TIME_MS)
        {
            control->last_cycle_duration_ms =
                    current_tick - last_state_tick;

            control->peltier_hot_fan_on = true;

            HAL_GPIO_WritePin(
                    control->error_led_port,
                    control->error_led_pin,
                    GPIO_PIN_SET);

            last_state_tick = current_tick;

            /* The completed phase was an OFF phase. */
            pending_learn = true;
            learn_for_off_phase = true;

            control->phase_reversed = false;

            /* Start classification for the new ON phase. */
            control->cold_fan_used_this_phase = false;
        }
    }
    else
    {
        /*
         * ON -> OFF only after the peak has been confirmed, humidity has
         * fallen to the anticipatory OFF threshold, and minimum ON time passed.
         */
        if (control->phase_reversed
                && control->filtered_hum <= turn_off_thres
                && (current_tick - last_state_tick)
                        >= MIN_ON_TIME_MS)
        {
            control->last_cycle_duration_ms =
                    current_tick - last_state_tick;

            control->peltier_hot_fan_on = false;

            HAL_GPIO_WritePin(
                    control->error_led_port,
                    control->error_led_pin,
                    GPIO_PIN_RESET);

            last_state_tick = current_tick;

            /* The completed phase was an ON phase. */
            pending_learn = true;
            learn_for_off_phase = false;

            control->phase_reversed = false;

            /*
             * cold_fan_used_this_phase is intentionally retained during OFF.
             * This associates the following trough with the preceding ON phase.
             */
        }
    }

    control_peltier_output(control);

    float dew_point_temp = dew_point_calculate(sht30);

    /*
     * Cold-fan hysteresis:
     *   ON  at plate <= dew point - 5 C while Peltier is ON.
     *   OFF at plate >  dew point - 2 C or whenever Peltier is OFF.
     * Between the two thresholds, preserve the previous fan state.
     */

    bool thermal_ready = ntc->temp <= (dew_point_temp - 4.0);
    bool humidity_demand = control->filtered_hum >= (control->target_hum + 1.0);
    bool cold_fan_on = thermal_ready && humidity_demand;
    bool thermal_not_ready = ntc->temp > (dew_point_temp - 2.0);
    bool humidity_not_demand = control->filtered_hum <= (control->target_hum + 0.3);
    bool cold_fan_off = thermal_not_ready || humidity_not_demand;
    if (cold_fan_on)
    {
        control_cold_fan(control, 50U);

        HAL_GPIO_WritePin(
                control->normal_led_port,
                control->normal_led_pin,
                GPIO_PIN_SET);

        control->debug_cold_fan = 1U;
        control->cold_fan_used_this_phase = true;
    }
    else if (cold_fan_off)
    {
        control_cold_fan(control, 0U);

        HAL_GPIO_WritePin(
                control->normal_led_port,
                control->normal_led_pin,
                GPIO_PIN_RESET);

        control->debug_cold_fan = 0U;
    }

    control->state_elapsed_ms =
            current_tick - last_state_tick;

    control->debug_dewpoint = dew_point_temp;
    control->debug_peltier =
            control->peltier_hot_fan_on ? 1U : 0U;
    control->debug_turn_on = turn_on_thres;
    control->debug_turn_off = turn_off_thres;
}

void control_filter_hum(
        control_t *control,
        sht30_t *sht30)
{
    control->filtered_hum =
            EMA_ALPHA * sht30->hum
            + (1.0f - EMA_ALPHA)
            * control->filtered_hum;
}
