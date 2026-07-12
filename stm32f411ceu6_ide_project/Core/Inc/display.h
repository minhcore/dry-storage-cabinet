#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "oled.h"
#include "fsm.h"
#include "sht30.h"
#include "control.h"

#define DEGREE 0x80

void display_update(state_e current_state, oled_t* oled, sht30_t* sht30, control_t* control);

#endif
