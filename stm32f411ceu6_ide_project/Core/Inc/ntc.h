#ifndef NTC_H_
#define NTC_H_

#include "stm32f4xx.h"
#include "stdbool.h"

#define NTC_VCC				3.3
#define NTC_ADC_MAX			4095.0
#define NTC_R_FIXED			9900.0 // 10 kohm has error of +- 1%
#define NTC_R0				10000.0 // MF-52-103 => 103 = 10 kohm at 25 C
#define	NTC_T0_K			298.15	// 25 C in Kevin
#define NTC_BETA			3950.0
#define NTC_SHORTED_THRES	50
#define NTC_OPEN_THRES		4025

typedef enum
{
	NTC_OK,
	NTC_ERROR
}ntc_status_e;

typedef struct
{
	ADC_HandleTypeDef* adc;
	uint32_t adc_raw;
	float resistance;
	float temp;
	ntc_status_e status;
	bool is_init;
}ntc_t;

ntc_status_e ntc_init(ntc_t* ntc, ADC_HandleTypeDef* adc);
ntc_status_e ntc_read_adc(ntc_t* ntc);
void ntc_calculate_temp(ntc_t* ntc);

#endif
