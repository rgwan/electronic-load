#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include "measure.h"
#include "lcd1602.h"

uint16_t REG_CH[128];
uint16_t V_CH[128];
uint16_t I_CH[128];
uint16_t adc_p = 0;
uint8_t ADC_CH[4]={1, 2, 17};
uint8_t ADC_CUR = 0;

uint16_t flt_p = 0;

uint32_t flt_u = 0;
uint32_t flt_i = 0;
uint32_t flt_r = 0;

uint16_t Measure_Current;
uint16_t Measure_Voltage;
uint16_t Measure_Reg;

#define K 9 * 3386
#define X 0

void ADC_Polling(void)
{
	if(adc_eoc(ADC1))
	{
		switch(ADC_CUR)
		{
			case 0:
				V_CH[adc_p] = adc_read_regular(ADC1);
			break;
			case 1:
				I_CH[adc_p] = adc_read_regular(ADC1);
			break;
			case 2:
				REG_CH[adc_p] = adc_read_regular(ADC1);
				adc_p++;
				if(adc_p >= 128) {
					adc_p = 0;
					Measure_Calculate();
				}
			break;
			default:
			break;
		}
		ADC_CUR++;
		if(ADC_CUR >= 3) ADC_CUR = 0;
		adc_set_regular_sequence(ADC1, 1, ADC_CH + ADC_CUR);
		adc_start_conversion_direct(ADC1);
	}
}

void Measure_Calculate()
{
	uint32_t reg_adc= 0;
	uint32_t i_adc = 0;
	uint32_t u_adc = 0;
	uint16_t reg_max = 0;
	uint16_t reg_min = -1;
	uint16_t u_max = 0;
	uint16_t u_min = -1;
	uint16_t i_max = 0;
	uint16_t i_min = -1;
	int i;
	for(i = 0; i < 128; i++)
	{
		/*if(reg_min > REG_CH[i]) reg_min = REG_CH[i];
		if(reg_max < REG_CH[i]) reg_max = REG_CH[i];
		if(i_min > I_CH[i]) reg_min = I_CH[i];
		if(i_max < I_CH[i]) reg_max = I_CH[i];
		if(u_min > V_CH[i]) reg_min = V_CH[i];
		if(u_max < V_CH[i]) reg_max = V_CH[i];*/
		reg_adc += REG_CH[i];
		i_adc += I_CH[i];
		u_adc += V_CH[i];
	}
	/*reg_adc -= reg_min + reg_max;
	i_adc -= i_min + i_max;
	u_adc -= u_min + u_max;*/
	reg_adc >>= 7;
	i_adc >>= 7;
	u_adc >>= 7;

	flt_p ++;
	if(flt_p >= 128)
	{
		Measure_Voltage = flt_u >> 5;
		Measure_Reg = flt_r >> 5;
		flt_p = 0;
		flt_u = 0;
		flt_i = 0;
		flt_r = 0;
	}
	flt_u += u_adc;
	flt_i += i_adc;
	flt_r += reg_adc;
}

uint16_t getVoltage(void)
{
	if(Measure_Voltage - X < 0)
		return 0;
	return (Measure_Voltage - X) * K / 16384;
}

uint16_t getReg(void)
{
	if(Measure_Reg - X < 0)
		return 0;
	return Measure_Reg * 3333 / 16384;
}
