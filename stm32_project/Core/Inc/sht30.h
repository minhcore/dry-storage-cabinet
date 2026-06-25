#ifndef SHT30_H_
#define SHT30_H_

#include "stm32f4xx.h"

#define SHT30_ADDR (0x44 << 1)

typedef enum
{
	SHT30_OK,
	SHT30_ERROR
} sht30_status_e;

typedef enum
{
	high_repeatability_mode = 0x2400,
	medium_repeatability_mode = 0x240b,
	low_repeatability_mode = 0x2416
}repeatability_e;

typedef struct
{
	I2C_HandleTypeDef* i2c;
	uint8_t address;
	repeatability_e repeat_mode;
	uint16_t raw_temp;
	uint16_t raw_hum;
	float temp;
	float hum;
	uint8_t expected_temp;
	uint8_t expected_hum;
}sht30_t;

sht30_status_e sht30_init(sht30_t *sht30, I2C_HandleTypeDef *i2c, uint8_t address, repeatability_e mode);
void sht30_config(sht30_t *sht30, repeatability_e new_mode);
sht30_status_e sht30_get(sht30_t *sht30);
void sht30_calculate(sht30_t *sht30);

#endif
