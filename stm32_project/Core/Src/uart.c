#include "uart.h"

void uart_send_char(char c)
{
	uint8_t data = (uint8_t)c;
	HAL_UART_Transmit(&huart2, &data, 1, HAL_MAX_DELAY);
}

void uart_send_string(char* buf)
{
	char* ptr = buf;
	while (*ptr != '\0')
	{
		uart_send_char(*ptr);
		ptr++;
	}
}

void uart_send_int(uint32_t num)
{
	char tmp_buf[32];
	int32_t i = 0;

	if (num == 0)
	{
		uart_send_char('0');
		return;
	}

	while (num > 0)
	{
		tmp_buf[i++] = '0' + (num % 10);
		num = num / 10;
	}
	i--;

	while (i >= 0)
	{
		uart_send_char(tmp_buf[i--]);
	}
}

