#include "sht30.h"

static uint8_t SOFT_RESET[2] = {0x30, 0xA2};

sht30_status_e sht30_init(sht30_t *sht30, I2C_HandleTypeDef *i2c, uint8_t address, repeatability_e mode)
{
	sht30->i2c = i2c;
	sht30->address = address;
	sht30->repeat_mode = mode;

	// Soft Reset
	HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(sht30->i2c, sht30->address, SOFT_RESET, 2, HAL_MAX_DELAY);
	if (status != HAL_OK) return SHT30_ERROR;
	HAL_Delay(2);
	return SHT30_OK;
}

void sht30_config(sht30_t *sht30, repeatability_e new_mode)
{
	sht30->repeat_mode = new_mode;
}

sht30_status_e sht30_get(sht30_t *sht30)
{
	HAL_StatusTypeDef status;

	// Sending Command
	uint8_t command[2];
	command[0] = (sht30->repeat_mode >> 8) & 0xFF;
	command[1] = sht30->repeat_mode & 0xFF;
	status = HAL_I2C_Master_Transmit(sht30->i2c, sht30->address, command, 2, HAL_MAX_DELAY);
	if (status != HAL_OK) return SHT30_ERROR;

	switch(sht30->repeat_mode)
	{
	case low_repeatability_mode:
		HAL_Delay(5);
		break;

	case medium_repeatability_mode:
		HAL_Delay(7);
		break;

	case high_repeatability_mode:
		HAL_Delay(16);
		break;
	}

	// Receive raw 16 bit temperature and humidity
	// temp(msb) + temp(lsb) + checksum + hum(msb) + hum(lsb) + checksum
	uint8_t data[6];
	status = HAL_I2C_Master_Receive(sht30->i2c, sht30->address, data, 6, HAL_MAX_DELAY);
	if (status != HAL_OK) return SHT30_ERROR;
	sht30->raw_temp = (data[0] << 8) | data[1];
	sht30->raw_hum = (data[3] << 8) | data[4];

	return SHT30_OK;
}

void sht30_calculate(sht30_t *sht30)
{
	sht30->temp = -45.0 + 175.0 * ((float)(sht30->raw_temp) / (float)((1 << 16) - 1));
	sht30->hum = 100.0 * ((float)(sht30->raw_hum) / (float)((1 << 16) - 1));
}


