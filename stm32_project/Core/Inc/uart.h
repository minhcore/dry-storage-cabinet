#ifndef UART_H_
#define UART_H_

#include "stm32f4xx.h"

extern UART_HandleTypeDef huart2;

void uart_send_char(char c);
void uart_send_string(char* buf);
void uart_send_int(uint32_t num);

#endif
