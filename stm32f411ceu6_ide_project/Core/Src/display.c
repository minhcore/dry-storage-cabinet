#include "display.h"
#include <string.h>

static uint32_t display_tick;
static uint8_t toggle = 0;

static void display_running(oled_t* oled, sht30_t* sht30, control_t* control)
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

static void set_hum_display(oled_t* oled, control_t* control, uint8_t tick, uint8_t cursor)
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
			toggle = (~toggle) & 0x01;
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
		display_running(oled, sht30, control);
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
	}
	oled_send_buffer(oled);
}


