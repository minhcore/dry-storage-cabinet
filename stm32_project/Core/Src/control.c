#include "control.h"

void control_init(control_t* control)
{
	control->target_temp = 25.0;
	control->target_hum = 50.0;
	control->temp_hysteresis = 2.0;
	control->hum_hysteresis = 2.0;
}
