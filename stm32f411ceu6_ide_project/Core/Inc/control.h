#ifndef CONTROL_H_
#define CONTROL_H_

#include <stdbool.h>
#include "stm32f4xx.h"
#include "ntc.h"
#include "sht30.h"

#define COLD_FAN_ARR              	999U
#define DEFAULT_TARGET_HUM			50.0		// %
#define DEFAULT_TEMP_LIMIT_ALARM	45			// C (degree)
#define DEFAULT_TEMP_DELAY_ALARM	0			// minutes
#define DEFAULT_HUM_MARGIN_ALARM	5			// %
#define DEFAULT_HUM_DELAY_ALARM		30			// minutes

#define LOW_ON_ADVANCE              0.1
#define LOW_OFF_ADVANCE           	0.3
#define LOW_MIN_ON_MS            	40000		// 40 seconds
#define LOW_MIN_OFF_MS              15000		// 15 seconds

#define MID_ON_ADVANCE              0.3
#define MID_OFF_ADVANCE             0.3
#define MID_MIN_ON_MS               40000		// 40 seconds
#define MID_MIN_OFF_MS              30000		// 30 seconds

#define HIGH_ON_ADVANCE             0.9
#define HIGH_OFF_ADVANCE            1.0
#define HIGH_MIN_ON_MS              40000		// 40 seconds
#define HIGH_MIN_OFF_MS             40000		// 20 seconds

#define CONDENSING_MARGIN			1.5			// Allow Peltier OFF only after condensation starts
#define COLD_FAN_ON_TEMP_MARGIN		4.0
#define COLD_FAN_OFF_TEMP_MARGIN	2.0
#define COLD_FAN_ON_HUM_MARGIN		1.0
#define COLD_FAN_OFF_HUM_MARGIN		0.3
#define COLD_FAN_FAR_ABOVE_OFFSET	3.0
#define COLD_FAN_BOOST_DELAY_MS		75000		// 75 seconds

typedef struct
{
	float on_advance;
	float off_advance;
	uint32_t min_on_ms;
	uint32_t min_off_ms;
	uint8_t	id;
}control_profile_t;

typedef struct
{
    // Cold fan PWM
    TIM_HandleTypeDef *tim;
    uint32_t tim_channel;

    // Peltier and Hot Fan
    GPIO_TypeDef *peltier_port;
    GPIO_TypeDef *hot_fan_port;
    uint16_t peltier_pin;
    uint16_t hot_fan_pin;

    // Peltier LED & Buzzer
    GPIO_TypeDef *peltier_led_port;
    uint16_t peltier_led_pin;
    GPIO_TypeDef *buzzer_port;
    uint16_t buzzer_pin;

    // Control Value
    float target_hum;
    bool peltier_on;
    bool cold_fan_on;
    bool control_started;
    uint32_t last_state_tick;
    uint32_t state_elapsed_ms;

    // Temp Alarm (If current temp reaches temp_limit, the alarm will go on)
    float temp_limit;
    bool is_alarm_temp;

	// Hum Alarm (If after hum_delay (time after starting machine or changing target), the current hum still exceeds the +-hum_margin, the alarm will go on)
    float hum_margin;
    float hum_delay_mins;	// minutes
    bool is_alarm_hum;

    // Buzzer
    bool is_buzzer;

    /* UART/live-plot debug values */
    float debug_turn_on;
    float debug_turn_off;
    float debug_dewpoint;

    uint8_t debug_peltier;
    uint8_t debug_cold_fan;
    uint8_t debug_profile;
    uint8_t debug_condensing_started;
    uint8_t debug_fan_boost_allowed;


} control_t;

void control_init(
        control_t *control,
        TIM_HandleTypeDef *tim,
        uint32_t tim_channel,
        GPIO_TypeDef *peltier_port,
        uint16_t peltier_pin,
        GPIO_TypeDef *hot_fan_port,
        uint16_t hot_fan_pin,
        GPIO_TypeDef *peltier_led_port,
        uint16_t peltier_led_pin);

void control_update(control_t *control ,ntc_t *ntc, sht30_t *sht30);

void control_alarm_hum_check(control_t *control, sht30_t *sht30);

void control_alarm_temp_check(control_t* control, sht30_t *sht30);

#endif /* CONTROL_H_ */
