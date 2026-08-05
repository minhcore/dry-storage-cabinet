#include "ntc.h"
#include "math.h"

void ntc_init(ntc_t* ntc, ADC_HandleTypeDef* adc)
{
	ntc->adc = adc;
}

ntc_status_e ntc_read_adc(ntc_t* ntc)
{
	HAL_StatusTypeDef status;

	status = HAL_ADC_Start(ntc->adc);
	if (status != HAL_OK) return NTC_ERROR;

	status = HAL_ADC_PollForConversion(ntc->adc, NTC_MAX_POLLING);
	if (status != HAL_OK) return NTC_ERROR;

	ntc->adc_raw = HAL_ADC_GetValue(ntc->adc);

	if ((ntc->adc_raw <= NTC_SHORTED_THRES) || (ntc->adc_raw > NTC_OPEN_THRES)) return NTC_ERROR; // ntc is shorted or open circuit

	return NTC_OK;
}

static void ntc_calculate_resistance(ntc_t* ntc)
{
	ntc->resistance = NTC_R_FIXED * (float)(ntc->adc_raw) / (NTC_ADC_MAX - (float)(ntc->adc_raw));
	return;
}

void ntc_calculate_temp(ntc_t* ntc)
{
	ntc_calculate_resistance(ntc);
	float inv_tmp = (1.0 / NTC_T0_K) + (1.0 / NTC_BETA) * logf(ntc->resistance / NTC_R0);
	ntc->temp = (1.0 / inv_tmp) - 273.15;
	return;
}
