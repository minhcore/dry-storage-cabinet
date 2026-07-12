#include "fsm.h"
#include <stdint.h>

static const transition_t fsm_table[] = {
		{RUNNING, PRESSED, SET_HUM},
		{RUNNING, ERROR_HAPPEN, ERROR_HANDLE},

		{SET_HUM, DOWN, SET_ALARM},
		{SET_HUM, PRESSED, SET_HUM_IDLE},
		{SET_ALARM, UP, SET_HUM},
		{SET_ALARM, DOWN, MENU_EXIT},
		{SET_ALARM, PRESSED, SET_ALARM_TEMP},
		{MENU_EXIT, UP, SET_ALARM},
		{MENU_EXIT, PRESSED, RUNNING},

		{SET_HUM_IDLE, DOWN, SET_ALARM},
		{SET_HUM_IDLE, PRESSED, SET_HUM_CHOOSE},
		{SET_HUM_CHOOSE, PRESSED, SET_HUM_IDLE},
		{SET_HUM_BACK, UP, SET_HUM_IDLE},
		{SET_HUM_BACK, PRESSED, SET_HUM},

		{SET_ALARM_TEMP, DOWN, SET_ALARM_HUM},
		{SET_ALARM_TEMP, PRESSED, SET_ALARM_CHOOSE_TEMP},
		{SET_ALARM_CHOOSE_TEMP, PRESSED, SET_ALARM_TEMP},
		{SET_ALARM_HUM, UP, SET_ALARM_TEMP},
		{SET_ALARM_HUM, DOWN, SET_ALARM_BUZZER},
		{SET_ALARM_HUM, PRESSED, SET_ALARM_CHOOSE_HUM},
		{SET_ALARM_CHOOSE_HUM, PRESSED, SET_ALARM_HUM},
		{SET_ALARM_BUZZER, UP, SET_ALARM_HUM},
		{SET_ALARM_BUZZER, DOWN, SET_ALARM_BACK},
		{SET_ALARM_BUZZER, PRESSED, SET_ALARM_CHOOSE_BUZZER},
		{SET_ALARM_CHOOSE_BUZZER, PRESSED, SET_ALARM_BUZZER},
		{SET_ALARM_BACK, UP, SET_ALARM_BUZZER},
		{SET_ALARM_BACK, PRESSED, SET_HUM},

};

static state_e current_state;

void fsm_init(void)
{
	current_state = RUNNING;
}
void fsm_run(event_e current_event)
{
	uint8_t table_size = sizeof(fsm_table) / sizeof(fsm_table[0]);

	if (current_event != NONE)
	{
		for (int i = 0; i < table_size; i++)
		{
			if (fsm_table[i].current_state == current_state
				&& fsm_table[i].event == current_event)
			{
				current_state = fsm_table[i].next_state;
				break;
			}
		}
	}
}
