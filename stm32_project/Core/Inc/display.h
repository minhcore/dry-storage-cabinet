#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "oled.h"
#include "sht30.h"
#include "control.h"

typedef enum
{
	DISPLAY_MAIN,
	DISPLAY_MENU,
	DISPLAY_SET_TARGET,
	DISPLAY_SET_TARGET_CHOOSE,
	DISPLAY_SET_HYSTERESIS,
	DISPLAY_SET_HYSTERESIS_CHOOSE
} display_state_e;

typedef enum
{
	CURSOR_MENU_SET_TARGET = 0,
	CURSOR_MENU_SET_HYSTERESIS,
	CURSOR_MENU_EXIT
} cursor_menu_e;

typedef enum
{
	CURSOR_SET_TARGET_TEMP = 0,
	CURSOR_SET_TARGET_HUM,
	CURSOR_SET_TARGET_BACK
} cursor_set_target_e;

typedef enum
{
	CURSOR_SET_HYSTERESIS_TEMP = 0,
	CURSOR_SET_HYSTERESIS_HUM,
	CURSOR_SET_HYSTERESIS_BACK
} cursor_set_hysteresis_e;

typedef struct
{
	volatile cursor_menu_e cursor_menu;
	volatile cursor_set_target_e cursor_set_target;
	volatile cursor_set_hysteresis_e cursor_set_hysteresis;
	volatile display_state_e state;
} display_t;

void display_init(display_t* display);
void display_task(display_t* display, oled_t* oled, sht30_t* sht30, control_t* control);

#endif
