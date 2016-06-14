#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/timer.h>
#include "measure.h"
#include "lcd1602.h"

uint16_t REG_CH[1024];
uint16_t V_CH[1024];
uint16_t I_CH[1024];
uint16_t adc_p = 0;
uint8_t ADC_CH[4]={1, 3, 17};
uint8_t ADC_CUR = 0;

uint16_t flt_p = 0;

uint32_t flt_u = 0;
uint32_t flt_i = 0;
uint32_t flt_r = 0;

uint16_t Measure_Current;
uint16_t Measure_Voltage;
uint16_t Measure_Reg;

uint8_t OT_Protected = 0;
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
				if(adc_p >= 1024) {
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
{/* We must have a FIR filter or a kalman filter */
	uint32_t reg_adc= 0;
	uint32_t i_adc = 0;
	uint32_t u_adc = 0;
	/*uint16_t reg_max = 0;
	uint16_t reg_min = -1;
	uint16_t u_max = 0;
	uint16_t u_min = -1;
	uint16_t i_max = 0;
	uint16_t i_min = -1;*/
	int i;
	for(i = 0; i < 1024; i++)
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
	reg_adc >>= 10;
	i_adc >>= 10;
	u_adc >>= 10;
	Measure_Voltage=u_adc;
	Measure_Reg =reg_adc;
	Measure_Current = i_adc;

	
}
/*
	TODO: Let this parameter can be adjust through special interface.
	Which requires a Flash driver. Now we just ignore it.
*/
uint16_t getVoltage(void)
{
	int v = Measure_Voltage;
	if(v -6 < 0)
		v = 0;

	else
		v -= 6;
	return v * Measure_Reg * 598 / 120000;//v * 10000 / 1350;
}

uint16_t getReg(void)
{

	return Measure_Reg;
}


uint16_t getCurrent(void)
{

	int i = Measure_Current;
	if(i -6 < 0)
		i = 0;
	else
		i -= 6;
		return i * Measure_Reg / 1200 * 1039 / 7620;// * 1000/ 5976;
		
}

void setCurrent(uint16_t i)
{
	uint64_t ccr = (uint64_t)Measure_Reg * i * 5100 / (uint64_t)1200000 + (i > 0 && i < 130 ? 21 : 0);
	if(i > 500)
		i = 0;
	if(OT_Protected)
		i = 0;
	if(getVoltage() > 24000)
		i = 0;
	TIM3_CCR3 = ccr & 0xffff;//602 * i / 1000 + (i > 0 ? 4 : 0);//6021 * 10066 / 100000;
}
