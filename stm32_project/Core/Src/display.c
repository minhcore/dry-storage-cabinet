#include "display.h"

static uint32_t blink_tick = 0;
static uint8_t is_blink = 1;

void display_task(display_t* display, oled_t* oled, sht30_t* sht30)
{
	uint16_t temp_x10;
	uint16_t hum_x10;
	uint8_t integer;
	uint8_t decimal;
	uint8_t point_column;
	uint8_t page;

	oled_clear_display(oled);
	switch(display->state)
	{
	case DISPLAY_MAIN:

		// Current temperature
		temp_x10	= (uint16_t)(sht30->temp * 10.0 + 0.5);
		integer = temp_x10 / 10;
		decimal = temp_x10 % 10;
		point_column = (integer < 10) ? 73 : 81;
		oled_draw_string(oled, "CUR T:  ", 0, 0);
		oled_draw_int(oled, integer, 0, 64);
		oled_draw_char(oled, '.', 0, point_column);
		oled_draw_int(oled, decimal, 0, point_column + 8);
		oled_draw_char(oled, OLED_DEGREE, 0, point_column + 16);
		oled_draw_char(oled, 'C', 0, point_column + 25);

		// Expected temperature
		oled_draw_string(oled, "SET T:  ", 4, 0);
		oled_draw_int(oled, sht30->expected_temp, 4, 64);
		oled_draw_char(oled, OLED_DEGREE, 4, 80);
		oled_draw_char(oled, 'C', 4, 88);

		// Current humidity
		hum_x10	= (uint16_t)(sht30->hum * 10.0 + 0.5);
		integer = hum_x10 / 10;
		decimal = hum_x10 % 10;
		point_column = (integer < 10) ? 73 : 81;
		oled_draw_string(oled, "CUR RH: ", 2, 0);
		oled_draw_int(oled, integer, 2, 64);
		oled_draw_char(oled, '.', 2, point_column);
		oled_draw_int(oled, decimal, 2, point_column + 8);
		oled_draw_char(oled, '%', 2, point_column + 24);

		// Expected humidity
		oled_draw_string(oled, "SET RH: ", 6, 0);
		oled_draw_int(oled, sht30->expected_hum, 6, 64);
		oled_draw_char(oled, '%', 6, 88);

		break;

	case DISPLAY_MENU:
		switch(display->cursor_menu)
		{
		case CURSOR_MENU_SET:
			page = 2;
			break;
		case CURSOR_MENU_EXIT:
			page = 4;
			break;
		}

		oled_draw_string(oled, "MENU:", 0, 16);
		oled_draw_string(oled, ">", page, 2);
		oled_draw_string(oled, "1.Set Target", 2, 16);
		oled_draw_string(oled, "2.Exit", 4, 16);

		break;

	case DISPLAY_SET_TARGET:
	case DISPLAY_SET_TARGET_CHOOSE:
		switch(display->cursor_set_target)
		{
		case CURSOR_SET_TARGET_TEMP:
			page = 2;
			break;
		case CURSOR_SET_TARGET_HUM:
			page = 4;
			break;
		case CURSOR_SET_TARGET_BACK:
			page = 6;
			break;
		}

		oled_draw_string(oled, "SET TARGET:", 0, 16);
		oled_draw_string(oled, ">", page, 2);

		oled_draw_string(oled, "1.T: ", 2, 16);
		oled_draw_int(oled, sht30->expected_temp, 2, 56);
		oled_draw_char(oled, OLED_DEGREE, 2, 72);
		oled_draw_char(oled, 'C', 2, 80);

		oled_draw_string(oled, "2.RH: ", 4, 16);
		oled_draw_int(oled, sht30->expected_hum, 4, 64);
		oled_draw_char(oled,'%', 4, 88);

		oled_draw_string(oled, "3.Back", 6, 16);

		if (display->state == DISPLAY_SET_TARGET_CHOOSE)
		{
			if (display->cursor_set_target == CURSOR_SET_TARGET_TEMP)
			{
				if (HAL_GetTick() - blink_tick >= 500)
				{
					if (is_blink) oled_draw_string(oled, "  ", page, 56);
					else oled_draw_int(oled, sht30->expected_temp, page, 56);
					is_blink = (~is_blink) & 0x01;
					blink_tick = HAL_GetTick();
				}
			}
			else if (display->cursor_set_target == CURSOR_SET_TARGET_HUM)
			{
				if (HAL_GetTick() - blink_tick >= 500)
				{
					if (is_blink) oled_draw_string(oled, "  ", page, 64);
					else oled_draw_int(oled, sht30->expected_hum, page, 64);
					is_blink = (~is_blink) & 0x01;
					blink_tick = HAL_GetTick();
				}
			}
		}

		break;

	}
	oled_send_buffer(oled);
}
