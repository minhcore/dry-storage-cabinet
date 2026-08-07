#include "fsm.h"
#include <stdint.h>

static const transition_t fsm_table[] = {
//	Current State					Event			Next State

	{RUNNING, 						PRESSED, 		SET_TARGET},
	{RUNNING,						ERROR_HAPPEN, 	ERROR_HANDLE},

	{SET_TARGET, 					DOWN, 			SET_ALARM},
	{SET_TARGET, 					PRESSED, 		SET_HUM_IDLE},
	{SET_ALARM, 					UP, 			SET_TARGET},
	{SET_ALARM, 					DOWN, 			MENU_EXIT},
	{SET_ALARM, 					PRESSED, 		SET_ALARM_TEMP},
	{MENU_EXIT, 					UP, 			SET_ALARM},
	{MENU_EXIT, 					PRESSED, 		RUNNING},

	{SET_HUM_IDLE, 					DOWN, 			SET_HUM_BACK},
	{SET_HUM_IDLE, 					PRESSED, 		SET_HUM_CHOOSE},
	{SET_HUM_CHOOSE, 				PRESSED, 		SET_HUM_IDLE},
	{SET_HUM_BACK, 					UP, 			SET_HUM_IDLE},
	{SET_HUM_BACK, 					PRESSED, 		SET_TARGET},

	{SET_ALARM_TEMP, 				DOWN, 			SET_ALARM_HUM},
	{SET_ALARM_TEMP, 				PRESSED, 		SET_ALARM_LIMIT_TEMP},
	{SET_ALARM_HUM, 				UP, 			SET_ALARM_TEMP},
	{SET_ALARM_HUM, 				DOWN, 			SET_ALARM_BUZZER},
	{SET_ALARM_HUM, 				PRESSED, 		SET_ALARM_MARGIN_HUM},
	{SET_ALARM_BUZZER, 				UP, 			SET_ALARM_HUM},
	{SET_ALARM_BUZZER, 				DOWN, 			SET_ALARM_BACK},
	{SET_ALARM_BUZZER, 				PRESSED, 		SET_ALARM_BUZZER_CHOOSE},
	{SET_ALARM_BUZZER_CHOOSE,		PRESSED,		SET_ALARM_BUZZER},
	{SET_ALARM_BACK, 				UP, 			SET_ALARM_BUZZER},
	{SET_ALARM_BACK, 				PRESSED, 		SET_TARGET},

	{SET_ALARM_LIMIT_TEMP, 			DOWN, 			SET_ALARM_TEMP_BACK},
	{SET_ALARM_LIMIT_TEMP, 			PRESSED, 		SET_ALARM_LIMIT_TEMP_CHOOSE},
	{SET_ALARM_LIMIT_TEMP_CHOOSE,	PRESSED,		SET_ALARM_LIMIT_TEMP},
	{SET_ALARM_TEMP_BACK, 			UP, 			SET_ALARM_LIMIT_TEMP},
	{SET_ALARM_TEMP_BACK, 			PRESSED, 		SET_ALARM_TEMP},

	{SET_ALARM_MARGIN_HUM, 			DOWN, 			SET_ALARM_DELAY_HUM},
	{SET_ALARM_MARGIN_HUM, 			PRESSED, 		SET_ALARM_MARGIN_HUM_CHOOSE},
	{SET_ALARM_MARGIN_HUM_CHOOSE,	PRESSED,		SET_ALARM_MARGIN_HUM},
	{SET_ALARM_DELAY_HUM, 			UP, 			SET_ALARM_LIMIT_TEMP},
	{SET_ALARM_DELAY_HUM, 			DOWN, 			SET_ALARM_HUM_BACK},
	{SET_ALARM_DELAY_HUM, 			PRESSED, 		SET_ALARM_DELAY_HUM_CHOOSE},
	{SET_ALARM_DELAY_HUM_CHOOSE,	PRESSED,		SET_ALARM_DELAY_HUM},
	{SET_ALARM_HUM_BACK, 			UP, 			SET_ALARM_DELAY_HUM},
	{SET_ALARM_HUM_BACK, 			PRESSED, 		SET_ALARM_TEMP},

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
		if (current_event == ERROR_HAPPEN)
		{
			current_state = ERROR_HANDLE;
			return;
		}

		for (int i = 0; i < table_size; i++)
		{
			if (fsm_table[i].current_state == current_state && fsm_table[i].event == current_event)
			{
				current_state = fsm_table[i].next_state;
				break;
			}
		}
	}
}

state_e fsm_get(void)
{
	return current_state;
}
