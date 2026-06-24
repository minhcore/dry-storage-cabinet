#include "encoder.h"

void encoder_init(encoder_t* encoder, TIM_HandleTypeDef* tim, uint16_t pin)
{
	encoder->tim = tim;
	encoder->current_value = 0;
	encoder->last_value = 0;
	encoder->status = ENCODER_NONE;
	encoder->button_pin = pin;
	encoder->debounce_tick = 0;
	encoder->is_pressed_button = 0;
	HAL_TIM_Encoder_Start(tim, TIM_CHANNEL_ALL);
}

encoder_status_t encoder_read(encoder_t* encoder)
{
	encoder_status_t status = ENCODER_NONE;

	encoder->current_value = __HAL_TIM_GET_COUNTER(encoder->tim);

	if (encoder->current_value >= (encoder->last_value + 4))
	{
		status = ENCODER_SCROLL_UP;
		encoder->last_value = encoder->current_value;
	}
	else if ((encoder->current_value + 4) <= encoder->last_value)
	{
		status = ENCODER_SCROLL_DOWN;
		encoder->last_value = encoder->current_value;
	}
	else if (encoder->is_pressed_button)
	{
		status = ENCODER_PRESSED;
		encoder->is_pressed_button = 0;
	}

	return status;
}




