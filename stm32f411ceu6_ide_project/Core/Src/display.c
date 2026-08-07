#include "display.h"
#include <string.h>

static uint32_t display_tick;
static bool toggle = false;

static void running_display(oled_t* oled, sht30_t* sht30, control_t* control)
{
	uint8_t current_x = 0;
	uint16_t hum = sht30->hum * 10;
	uint16_t temp = sht30->temp * 10;
	uint16_t set_hum = control->target_hum;

	oled_draw_string(oled, "CUR HUM: ", 0, current_x);
	current_x += (strlen("CUR HUM: ") * 8);
	oled_draw_int(oled, hum / 10, 0, current_x);
	current_x += ((hum / 10) < 10) ? 8 : 16;
	oled_draw_char(oled, '.', 0, current_x);
	current_x += 8;
	oled_draw_int(oled, hum % 10, 0, current_x);
	current_x += 8;
	oled_draw_string(oled," %", 0, current_x);

	current_x = 0;
	oled_draw_string(oled, "CUR T: ", 2, current_x);
	current_x += (strlen("CUR T: ") * 8) + 16;
	oled_draw_int(oled, temp / 10, 2, current_x);
	current_x += ((temp / 10) < 10) ? 8 : 16;
	oled_draw_char(oled, '.', 2, current_x);
	current_x += 8;
	oled_draw_int(oled, temp % 10, 2, current_x);
	current_x += 8;
	oled_draw_char(oled, DEGREE, 2, current_x);
	current_x += 8;
	oled_draw_char(oled, 'C', 2, current_x);

	current_x = 0;
	oled_draw_string(oled, "SET HUM: ", 4, current_x);
	current_x += (strlen("SET HUM: ") * 8);
	oled_draw_int(oled, set_hum, 4, current_x);
	current_x += 16;
	oled_draw_string(oled," %", 4, current_x);

	if(control->is_alarm_hum && control->is_alarm_temp) oled_draw_string(oled, "ALARM H&T!", 6, 0);
	else if (control->is_alarm_hum) oled_draw_string(oled, "ALARM HUM!", 6, 0);
	else if (control->is_alarm_temp) oled_draw_string(oled, "ALARM TEMP!", 6, 0);
	else oled_draw_string(oled, "STATUS: OK", 6, 0);

}

static void draw_cursor(oled_t* oled, uint8_t cursor)
{
	oled_draw_char(oled, '>', cursor, 2);
}

static void menu_display(oled_t* oled, uint8_t cursor)
{
	oled_draw_string(oled, "MENU:", 0, 16);
	oled_draw_string(oled, "1.Set Target", 2, 16);
	oled_draw_string(oled, "2.Set Alarm", 4, 16);
	oled_draw_string(oled, "3.Exit", 6, 16);
	draw_cursor(oled, cursor);

}

static void set_hum_display(oled_t* oled, control_t* control, bool tick, uint8_t cursor)
{
	uint8_t current_x = 16;
	oled_draw_string(oled, "SET TARGET:", 0, 16);
	oled_draw_string(oled, "1.Hum: ", 2, current_x);
	current_x += strlen("1.Hum: ") * 8;
	oled_draw_int(oled, control->target_hum, 2, current_x);
	current_x += 16;
	oled_draw_string(oled, " %", 2, current_x);
	oled_draw_string(oled, "2.Back", 4, 16);
	draw_cursor(oled, cursor);

	if (tick)
	{
		uint32_t wait_tick = (toggle) ? 700 : 300;
		if ((HAL_GetTick() - display_tick) >= wait_tick)
		{
			if (toggle) oled_draw_char(oled, ':', 2, 56);
			else oled_draw_char(oled, ' ', 2, 56);
			toggle = !toggle;
			display_tick = HAL_GetTick();
		}
	}
}

static void error_display(oled_t* oled)
{
	oled_draw_string(oled, "SYSTEM ERROR!", 0, 0);
	oled_draw_string(oled, "CHECK SENSOR!", 2, 0);
	oled_draw_string(oled, "RESET REQUIRED!", 4, 0);
}

static void set_alarm_display(oled_t* oled, control_t* control, uint8_t cursor, bool tick)
{
	uint8_t current_x = 16;
	oled_draw_string(oled, "SET ALARM:", 0, 16);
	draw_cursor(oled, cursor);
	oled_draw_string(oled, "1.Temp", 2, 16);
	oled_draw_string(oled, "2.Hum", 4, 16);
	oled_draw_string(oled, "3.Buzzer: ", 6, 16);
	current_x += strlen("3.Buzzer: ") * 8;

	if (control->is_buzzer) oled_draw_string(oled, "ON", 6, current_x);
	else oled_draw_string(oled, "OFF", 6, current_x);

	if (tick)
	{
		uint32_t wait_tick = (toggle) ? 700 : 300;
		if ((HAL_GetTick() - display_tick) >= wait_tick)
		{
			if (toggle) oled_draw_char(oled, ':', 6, 80);
			else oled_draw_char(oled, ' ', 6, 80);
			toggle = !toggle;
			display_tick = HAL_GetTick();
		}
	}
}

static void set_alarm_back_display(oled_t* oled)
{
	oled_draw_string(oled, "SET ALARM:", 0, 16);
	oled_draw_string(oled, "> 4.Back", 2, 0);
}

static void set_alarm_temp_display(oled_t* oled, control_t* control, uint8_t cursor, bool tick)
{
	uint8_t current_x = 16;
	oled_draw_string(oled, "TEMP ALARM:", 0, 16);
	draw_cursor(oled, cursor);
	oled_draw_string(oled, "1.Limit: ", 2, current_x);
	current_x += strlen("1.Limit: ") * 8;
	oled_draw_int(oled, control->temp_limit, 2, current_x);
	current_x += 16;
	oled_draw_char(oled, DEGREE, 2, current_x);
	current_x += 8;
	oled_draw_char(oled, 'C', 2, current_x);
	oled_draw_string(oled, "2.Back", 4, 16);

	if (tick)
	{
		uint32_t wait_tick = (toggle) ? 700 : 300;
		if ((HAL_GetTick() - display_tick) >= wait_tick)
		{
			if (toggle) oled_draw_char(oled, ':', 2, 72);
			else oled_draw_char(oled, ' ', 2, 72);
			toggle = !toggle;
			display_tick = HAL_GetTick();
		}
	}
}

static void set_alarm_hum_display(oled_t* oled, control_t* control, uint8_t cursor, bool tick)
{
	uint8_t current_x = 16;
	oled_draw_string(oled, "HUM ALARM:", 0, 16);
	draw_cursor(oled, cursor);
	oled_draw_string(oled, "1.Margin: ", 2, current_x);
	current_x += strlen("1.Margin: ") * 8;
	oled_draw_int(oled, control->hum_margin, 2, current_x);
	current_x += 24;
	oled_draw_char(oled, '%', 2, current_x);
	current_x = 16; // Reset
	oled_draw_string(oled, "2.Delay:", 4, current_x);
	current_x += strlen("2.Delay:") * 8;
	oled_draw_int(oled, control->hum_delay_mins, 4, current_x);
	current_x += 16;
	oled_draw_string(oled, "mins", 4, current_x);
	oled_draw_string(oled, "3.Back", 6, 16);

	if (tick)
	{
		uint32_t wait_tick = (toggle) ? 700 : 300;
		if ((HAL_GetTick() - display_tick) >= wait_tick)
		{
			if (cursor == 2) // At 1.Margin
			{
				if (toggle) oled_draw_char(oled, ':', 2, 80);
				else oled_draw_char(oled, ' ', 2, 80);
			}
			else if (cursor == 4) // At 2. Delay:
			{
				if (toggle) oled_draw_char(oled, ':', 4, 72);
				else oled_draw_char(oled, ' ', 4, 72);
			}
			toggle = !toggle;
			display_tick = HAL_GetTick();
		}
	}
}

void display_update(state_e current_state, oled_t* oled, sht30_t* sht30, control_t* control)
{

	// draw menu display
	oled_clear_display(oled);
	switch(current_state)
	{
	case RUNNING:
		running_display(oled, sht30, control);
		break;
	case ERROR_HANDLE:
		error_display(oled);
		break;
	case SET_TARGET:
		menu_display(oled, 2);
		break;
	case SET_ALARM:
		menu_display(oled, 4);
		break;
	case MENU_EXIT:
		menu_display(oled, 6);
		break;
	case SET_HUM_IDLE:
		set_hum_display(oled, control, 0, 2);
		break;
	case SET_HUM_CHOOSE:
		set_hum_display(oled, control, 1, 2);
		break;
	case SET_HUM_BACK:
		set_hum_display(oled, control, 0, 4);
		break;
	case SET_ALARM_TEMP:
		set_alarm_display(oled, control, 2, 0);
		break;
	case SET_ALARM_HUM:
		set_alarm_display(oled, control, 4, 0);
		break;
	case SET_ALARM_BUZZER:
		set_alarm_display(oled, control, 6, 0);
		break;
	case SET_ALARM_BUZZER_CHOOSE:
		set_alarm_display(oled, control, 6, 1);
		break;
	case SET_ALARM_BACK:
		set_alarm_back_display(oled);
		break;
	case SET_ALARM_LIMIT_TEMP:
		set_alarm_temp_display(oled, control, 2, 0);
		break;
	case SET_ALARM_LIMIT_TEMP_CHOOSE:
		set_alarm_temp_display(oled, control, 2, 1);
		break;
	case SET_ALARM_TEMP_BACK:
		set_alarm_temp_display(oled, control, 4, 0);
		break;
	case SET_ALARM_MARGIN_HUM:
		set_alarm_hum_display(oled, control, 2, 0);
		break;
	case SET_ALARM_MARGIN_HUM_CHOOSE:
		set_alarm_hum_display(oled, control, 2, 1);
		break;
	case SET_ALARM_DELAY_HUM:
		set_alarm_hum_display(oled, control, 4, 0);
		break;
	case SET_ALARM_DELAY_HUM_CHOOSE:
		set_alarm_hum_display(oled, control, 4, 1);
		break;
	case SET_ALARM_HUM_BACK:
		set_alarm_hum_display(oled, control, 6, 0);
		break;
	}
}


