#include "oled.h"
#include "font_8x12.h"

static uint8_t command_byte = 0x00;
static uint8_t data_byte = 0x40;
static uint8_t set_column_page[] =
{
		0x21, 0x00, 0x7F,
		0x22, 0x00, 0x07
};

static uint8_t oled_buffer[1024];

oled_status_e oled_init(oled_t* oled, I2C_HandleTypeDef* i2c, uint8_t address)
{
	HAL_StatusTypeDef status;
	oled->i2c = i2c;
	oled->address = address;
	oled->buffer = oled_buffer;
	static uint8_t init_buf[] = {
//			0x20, 0x00,			// Set Horizontal Addressing Mode
//			0x21, 0x00, 0x7F,	// Set Column Address from 0 to 127
//			0x22, 0x00, 0x07,	// Set Page Address from 0 to 7
//			0xa0,				// Set Segment Re-map
//			0xc0,				// Set COM Output Scan Direction
//			0xaf,				// Set Display ON
//			0xa5				// Set Resume to RAM content display
			0xa8, 0x3f, 		// Set Mux Ratio
			0xd3, 0x00, 		// Set Display Offset
			0x40, 				// Set Display Start Line
			0xa1, 				// Set Segment re-map
			0xc8, 				// Set COM Output Scan Direction
			0xda, 0x12, 		// Set COM Pins hardware configuration
			0x81, 0x7f, 		// Set Contrast Control
			0xa4, 				// Display Entire Display On
			0xa6, 				// Set Normal Display
			0xd5, 0x80, 		// Set Osc Frequency
			0x8d, 0x14, 		// Enable charge pump regulator
			0x20, 0x20, 		// Set Horizontal Adressing Mode
			0xaf 				// Display On
	};

	status = HAL_I2C_Mem_Write(oled->i2c, oled->address, command_byte, I2C_MEMADD_SIZE_8BIT, init_buf, sizeof(init_buf), 100);
	if (status != HAL_OK) return OLED_ERROR;
	return OLED_OK;
}

void oled_clear_display(oled_t* oled)
{
	memset(oled->buffer, 0, 1024);
}

oled_status_e oled_send_buffer(oled_t* oled)
{
	HAL_StatusTypeDef status;

	// Set pointer at 0,0
	status = HAL_I2C_Master_Transmit(oled->i2c, oled->address, set_column_page, sizeof(set_column_page), 100);
	if (status != HAL_OK) return OLED_ERROR;

	status = HAL_I2C_Mem_Write(oled->i2c, oled->address, (uint8_t)(data_byte), I2C_MEMADD_SIZE_8BIT, oled->buffer, OLED_BUFFER_SIZE, 1000);
	if (status != HAL_OK) return OLED_ERROR;
	return OLED_OK;
}

void oled_draw_char(oled_t* oled, char c, uint8_t page, uint8_t x)
{
	if (page >= 8 || x >= 128) return;

	const uint8_t *p = NULL;

	for (int i = 0; i < FONT_8X12_COUNT; i++)
	{
		if (c == font_8x12[i].ascii_code)
		{
			p = font_8x12[i].bitmap;
			break;
		}
	}
	if (p == NULL) return;

	if (((x + 8) <= 128) && ((page + 1) < 8))
	{
		uint16_t top_index = (page * 128) + x;
		uint16_t bot_index = ((page + 1) * 128) + x;

		for (int i = 0; i < 8; i++)
		{

			oled->buffer[top_index + i] = p[i];
			oled->buffer[bot_index + i] = p[i+8];

		}
	}
	else return;
}

void oled_draw_string(oled_t* oled, char* buf, uint8_t page, uint8_t x)
{
	char* p = buf;
	uint8_t x_value = x;
	while (*p != '\0')
	{
		oled_draw_char(oled, *p, page, x_value);
		p++;
		x_value += 8;
	}
}

void oled_draw_int(oled_t* oled, uint32_t number, uint8_t page, uint8_t x)
{
	char tmp_buf[32];
	int32_t i = 0;
	uint8_t x_value = x;

	if (number == 0)
	{
		oled_draw_char(oled, '0', page, x);
		return;
	}

	while (number > 0)
	{
		tmp_buf[i++] = '0' + (number % 10);
		number = number / 10;
	}
	i--;

	while (i>=0)
	{
		oled_draw_char(oled, tmp_buf[i--], page, x_value);
		x_value += 8;
	}
}

void oled_turn_all(oled_t *oled)
{
	memset(oled->buffer, 0xff, 1024);
}




