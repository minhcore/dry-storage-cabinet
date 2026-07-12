#include "display.h"
#include <string.h>

void display_update(state_e current_state, oled_t* oled, sht30_t* sht30, control_t* control)
{
	uint8_t current_x;

	// draw menu display
	oled_clear_display(oled);
	switch(current_state)
	{
	case RUNNING:
		current_x = 0;
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
		break;
	}
	//oled_send_buffer(oled);
}


