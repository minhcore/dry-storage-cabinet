#ifndef ENCODER_H_
#define ENCODER_H_

#include "stm32f4xx.h"

typedef enum
{
	ENCODER_NONE,
	ENCODER_SCROLL_UP,
	ENCODER_SCROLL_DOWN,
	ENCODER_PRESSED
} encoder_status_t;

typedef struct
{
	TIM_HandleTypeDef* tim;
	volatile int32_t current_value;
	volatile int32_t last_value;
	volatile encoder_status_t status;
	uint16_t button_pin;
	volatile uint8_t is_pressed_button;
	volatile uint32_t debounce_tick;
} encoder_t;

void encoder_init(encoder_t* encoder, TIM_HandleTypeDef* tim, uint16_t pin);
encoder_status_t encoder_read(encoder_t* encoder);

#endif
