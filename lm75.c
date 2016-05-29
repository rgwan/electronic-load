#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <stdio.h>
#include "lm75.h"

int16_t lm75_read(uint32_t i2c, uint8_t sensor)
{
	uint32_t reg32 __attribute__((unused));
	int16_t temperature = -1;
	uint32_t time_flag = 0;

	/* Send START condition. */
	i2c_send_start(i2c);
	/* Waiting for START is send and switched to master mode. */
	time_flag = 10000;
	while (!((I2C_SR1(i2c) & I2C_SR1_SB)
		& (I2C_SR2(i2c) & (I2C_SR2_MSL | I2C_SR2_BUSY))))
	{
		time_flag--;
		if(time_flag == 0) goto exit;
	}
	/* Say to what address we want to talk to. */
	/* Yes, WRITE is correct - for selecting register in STTS75. */
	i2c_send_7bit_address(i2c, sensor, I2C_WRITE);
	/* Waiting for address is transferred. */
	while (!(I2C_SR1(i2c) & I2C_SR1_ADDR))
	{
		time_flag--;
		if(time_flag == 0) goto exit;
	}
	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(i2c);

	i2c_send_data(i2c, 0x0); /* temperature register */
	while (!(I2C_SR1(i2c) & (I2C_SR1_BTF | I2C_SR1_TxE)))
	{
		time_flag--;
		if(time_flag == 0) goto exit;
	}

	/*
	 * Now we transferred that we want to ACCESS the temperature register.
	 * Now we send another START condition (repeated START) and then
	 * transfer the destination but with flag READ.
	 */

	/* Send START condition. */
	i2c_send_start(i2c);

	/* Waiting for START is send and switched to master mode. */
	while (!((I2C_SR1(i2c) & I2C_SR1_SB)
		& (I2C_SR2(i2c) & (I2C_SR2_MSL | I2C_SR2_BUSY))))
	{
		time_flag--;
		if(time_flag == 0) goto exit;
	}

	/* Say to what address we want to talk to. */
	i2c_send_7bit_address(i2c, sensor, I2C_READ); 

	/* 2-byte receive is a special case. See datasheet POS bit. */
	I2C_CR1(i2c) |= (I2C_CR1_POS | I2C_CR1_ACK);

	/* Waiting for address is transferred. */
	while (!(I2C_SR1(i2c) & I2C_SR1_ADDR))
	{
		time_flag--;
		if(time_flag == 0) goto exit;
	}

	/* Cleaning ADDR condition sequence. */
	reg32 = I2C_SR2(i2c);

	/* Cleaning I2C_SR1_ACK. */
	I2C_CR1(i2c) &= ~I2C_CR1_ACK;

	/* Now the slave should begin to send us the first byte. Await BTF. */
	while (!(I2C_SR1(i2c) & I2C_SR1_BTF))
	{
		time_flag--;
		if(time_flag == 0) goto exit;
	}
	temperature = (uint16_t)(I2C_DR(i2c) << 1); /* MSB */

	/*
	 * Yes they mean it: we have to generate the STOP condition before
	 * saving the 1st byte.
	 */
	

	temperature |= (uint16_t)(I2C_DR(i2c) >> 7); /* LSB */

exit:
	I2C_CR1(i2c) |= I2C_CR1_STOP;
	/* Original state. */
	if(time_flag == 0)
	{
		/*I2C_CR1(i2c) |= I2C_CR1_SWRST;
		delay(100);
		I2C_CR1(i2c) &= ~I2C_CR1_SWRST;*/
		i2c_peripheral_disable(I2C1);
		i2c_peripheral_enable(I2C1);
	}
	I2C_CR1(i2c) &= ~I2C_CR1_POS;

	return temperature;
}
