#ifndef FSM_H_
#define FSM_H_

typedef enum
{
	// Main state
	RUNNING,
	ERROR_HANDLE,

	// Menu setting state
	SET_TARGET,
	SET_ALARM,
	MENU_EXIT,

	// Humidity setting state
	SET_HUM_IDLE,
	SET_HUM_CHOOSE,
	SET_HUM_BACK,

	// Set Alarm setting state
	SET_ALARM_TEMP,
	SET_ALARM_HUM,
	SET_ALARM_CHOOSE_TEMP,
	SET_ALARM_CHOOSE_HUM,
	SET_ALARM_BUZZER,
	SET_ALARM_CHOOSE_BUZZER,
	SET_ALARM_BACK,
	
	// ...
} state_e;
typedef enum
{
	UP,
	DOWN,
	PRESSED,
	NONE,
	ERROR_HAPPEN
} event_e;

typedef struct
{
	state_e current_state;
	event_e event;
	state_e next_state;
} transition_t;

void fsm_init(void);
void fsm_run(event_e current_event);
state_e fsm_get(void);

#endif
