#include "tim.h"
#include "adc.h"

uint8_t convert_flag = 0;
typedef struct{
	uint32_t adc1;
	uint32_t adc2;
}ADC_Value;

ADC_Value my_adc_value;

uint32_t adc_buf[2] = {0};

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM3)
	{
		if(convert_flag == 1){
				HAL_ADC_Start_IT(&hadc1);  // need start ADC1 interrupt
				my_adc_value.adc1 = HAL_ADC_GetValue(&hadc1);  // store adc1 channel1 convert value
		}
		
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	my_adc_value.adc2 = HAL_ADC_GetValue(&hadc2);	// store adc2 channel1 convert value
}