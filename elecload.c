#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/cm3/nvic.h>
#include <stdio.h>
#include "lcd1602.h"
#include "lm75.h"
#include "measure.h"

/*
	PB12~PB15, PB8,9,10 LCD
	PB0 PWM
	PB6 I2C SCL
	PB7 I2C SDA
	PB1 FAN
	PA4~PA7 KEY
	PB5 LIGHT
	PB4 OVP/STATUS
*/

/* Set STM32 to 72 MHz. */
static void clock_setup(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	/* Enable TIM3 clock */
	rcc_periph_clock_enable(RCC_TIM3);
	/* Enable clocks for I2C1 and AFIO. */
	rcc_periph_clock_enable(RCC_I2C1);
	/* Enable GPIOC clock. */
	rcc_periph_clock_enable(RCC_GPIOB);

	rcc_periph_clock_enable(RCC_AFIO);

	rcc_periph_clock_enable(RCC_DMA1);

	rcc_periph_clock_enable(RCC_ADC1);

	rcc_periph_clock_enable(RCC_GPIOA);
}

static void gpio_setup(void)
{
	gpio_clear(GPIOB, GPIO0);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, 0xff00 | GPIO0); // for LCD and heartbeat and PWM0(Iset)
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
		      GPIO_TIM3_CH3);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
		      GPIO_I2C1_SCL | GPIO_I2C1_SDA);
	
}

static void tim_setup(void)
{
	/* Clock division and mode */
	TIM3_CR1 = TIM_CR1_CKD_CK_INT | TIM_CR1_CMS_EDGE;
	/* Period */
	TIM3_ARR = 4096;
	/* Prescaler */
	TIM3_PSC = 0;
	TIM3_EGR = TIM_EGR_UG;
#if 0
	/* ---- */
	/* Output compare 1 mode and preload */
	TIM3_CCMR1 |= TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC1PE;

	/* Polarity and state */
	TIM3_CCER |= TIM_CCER_CC1P | TIM_CCER_CC1E;
	//TIM3_CCER |= TIM_CCER_CC1E;

	/* Capture compare value */
	TIM3_CCR1 = 0;

	/* ---- */
	/* Output compare 2 mode and preload */
	TIM3_CCMR1 |= TIM_CCMR1_OC2M_PWM1 | TIM_CCMR1_OC2PE;

	/* Polarity and state */
	TIM3_CCER |= TIM_CCER_CC2P | TIM_CCER_CC2E;
	//TIM3_CCER |= TIM_CCER_CC2E;

	/* Capture compare value */
	TIM3_CCR2 = 0;
#endif
	/* ---- */
	/* Output compare 3 mode and preload */
	TIM3_CCMR2 |= TIM_CCMR2_OC3M_PWM1 | TIM_CCMR2_OC3PE;

	/* Polarity and state */
	TIM3_CCER |= TIM_CCER_CC3E; // TIM_CCER_CC3N | 
	TIM3_CCER &= ~(TIM_CCER_CC3P);
	//TIM3_CCER |= TIM_CCER_CC3E;

	/* Capture compare value */
	TIM3_CCR3 = 0;

	/* ---- */
#if 0
	/* Output compare 4 mode and preload */
	TIM3_CCMR2 |= TIM_CCMR2_OC4M_PWM1 | TIM_CCMR2_OC4PE;

	/* Polarity and state */
	TIM3_CCER |= TIM_CCER_CC4P | TIM_CCER_CC4E;
	//TIM3_CCER |= TIM_CCER_CC4E;

	/* Capture compare value */
	TIM3_CCR4 = 0;
#endif
	/* ---- */
	/* ARR reload enable */
	TIM3_CR1 |= TIM_CR1_ARPE;

	/* Counter enable */
	TIM3_CR1 |= TIM_CR1_CEN;
}

static void i2c_setup(void)
{
	i2c_peripheral_disable(I2C1);
	i2c_set_clock_frequency(I2C1, I2C_CR2_FREQ_36MHZ);
	/* 400KHz - I2C Fast Mode */
	i2c_set_fast_mode(I2C1);

	/*
	 * fclock for I2C is 36MHz APB2 -> cycle time 28ns, low time at 400kHz
	 * incl trise -> Thigh = 1600ns; CCR = tlow/tcycle = 0x1C,9;
	 * Datasheet suggests 0x1e.
	 */
	i2c_set_ccr(I2C1, 0x1e);

	/*
	 * fclock for I2C is 36MHz -> cycle time 28ns, rise time for
	 * 400kHz => 300ns and 100kHz => 1000ns; 300ns/28ns = 10;
	 * Incremented by 1 -> 11.
	 */
	i2c_set_trise(I2C1, 0x0b);

	/*
	 * This is our slave address - needed only if we want to receive from
	 * other masters.
	 */
	i2c_set_own_7bit_slave_address(I2C1, 0x32);

	/* If everything is configured -> enable the peripheral. */
	i2c_peripheral_enable(I2C1);
}

static void adc_setup(void)
{
	volatile int i;
	gpio_clear(GPIOB, GPIO0 | GPIO1 | GPIO2 | GPIO3);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO0 | GPIO3);
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO1);
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO2);
	/* Make sure the ADC doesn't run during config. */
	adc_power_off(ADC1);
	rcc_periph_reset_pulse(RST_ADC1);
    rcc_set_adcpre(RCC_CFGR_ADCPRE_PCLK2_DIV2);

	adc_disable_scan_mode(ADC1);
	//adc_disable_continuous_conversion_mode(ADC1);
	adc_disable_external_trigger_regular(ADC1);
	adc_set_right_aligned(ADC1);
	//adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5CYC);
	adc_set_sample_time(ADC1, ADC_CHANNEL17, ADC_SMPR_SMP_239DOT5CYC);
	adc_set_sample_time(ADC1, ADC_CHANNEL1, ADC_SMPR_SMP_239DOT5CYC);
	adc_set_sample_time(ADC1, ADC_CHANNEL2, ADC_SMPR_SMP_239DOT5CYC);
	adc_enable_temperature_sensor(ADC1);	
	adc_set_right_aligned(ADC1);
	//adc_set_dual_mode(ADC_CR1_DUALMOD_IND);
	adc_power_on(ADC1);

	/* Wait for ADC starting up. */
	for (i = 0; i < 800000; i++)    /* Wait a bit. */
		__asm__("nop");
	adc_reset_calibration(ADC1);
	adc_calibration(ADC1);
	
	adc_set_regular_sequence(ADC1, 1, ADC_CH);
	adc_start_conversion_direct(ADC1);
	i = 262144;
	while(i--)
		ADC_Polling();
}


int main(void)
{
	volatile int i = 0;
	uint16_t voltage;
	char str[32];
	int temp = 0;
	clock_setup();
	gpio_setup();
	LCD_Init();
	ClearLcdData();
	SetLcdXY(0, 0);
	puts_lcd("CDU-LOAD 1.00B");
	SetLcdXY(0, 1);
	puts_lcd("Initialzing Sys");
	delay(800000);
	tim_setup();
	SetLcdXY(0, 1);
	puts_lcd("Initialzed DAC");
	delay(800000);
	adc_setup();
	SetLcdXY(0, 1);
	puts_lcd("Initialzed ADC");
	i2c_setup();
	SetLcdXY(0, 1);
	puts_lcd("InitialzdSensor");	
	delay(800000);
	SetLcdXY(0, 0);
	puts_lcd("Is=0.00AIc=0.00A");
	TIM3_CCR3 = 2048;
	
	while (1) {
		i++;
		ADC_Polling();
		if(i >= 80000)
		{
			
			
#if 0
			/* Wait for end of conversion. */
			while (!(adc_eoc(ADC1)));

			voltage = adc_read_regular(ADC1);
			adc_start_conversion_direct(ADC1);

			/* Wait for end of conversion. */
			while (!(adc_eoc(ADC1)));
			temp = adc_read_regular(ADC1);
#endif
			//temp = lm75_read(I2C1, 0x48);
			//sprintf(str, "Uc=%dVT=%d.%dC", voltage, temp / 2, temp % 2 * 5);		
			SetLcdXY(0, 1);
			sprintf(str, "Uc=%dR=%d  ", getVoltage(), getReg());;		
			puts_lcd(str);
			
			i = 0;
		}

	}

	return 0;
}
