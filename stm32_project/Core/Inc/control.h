#ifndef CONTROL_H_
#define CONTROL_H_

typedef struct
{
	float target_temp;
	float target_hum;

	float temp_hysteresis;
	float hum_hysteresis;
} control_t;

void control_init(control_t* control);

#endif
