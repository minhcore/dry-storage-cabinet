#ifndef OLED_H_
#define OLED_H_

#include "stm32f4xx.h"
#include "string.h"
#include "stddef.h"

#define OLED_ADDR (0x3C << 1)
#define OLED_BUFFER_SIZE 1024
#define OLED_DEGREE 0x80

typedef enum
{
	OLED_OK,
	OLED_ERROR
}oled_status_e;

typedef struct
{
	I2C_HandleTypeDef* i2c;
	uint8_t address;
	uint8_t* buffer;
}oled_t;

oled_status_e oled_init(oled_t* oled, I2C_HandleTypeDef* i2c, uint8_t address);
void oled_clear_display(oled_t* oled);
oled_status_e oled_send_buffer(oled_t* oled);
void oled_draw_char(oled_t* oled, char c,uint8_t page, uint8_t x);
void oled_draw_string(oled_t* oled, char* buf, uint8_t page, uint8_t x);
void oled_draw_int(oled_t* oled, uint32_t number, uint8_t page, uint8_t x);
void oled_turn_all(oled_t* oled);

#endif
