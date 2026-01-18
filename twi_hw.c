/*
 * twi_hw.h
 *
 * Created: 11/01/2026 21:00:49
 *  Author: franc
 */ 


#define F_CPU 1000000UL



#include <avr/io.h>
#include <util/delay.h>
#include "twi_hw.h"

static uint8_t i2c_start(uint8_t address_rw)
{
	TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
	while (!(TWCR & (1<<TWINT)));

	TWDR = address_rw;
	TWCR = (1<<TWINT) | (1<<TWEN);
	while (!(TWCR & (1<<TWINT)));
	return 0;
}

static void i2c_stop(void)
{
	TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	_delay_us(10);
}

static uint8_t i2c_write(uint8_t data)
{
	TWDR = data;
	TWCR = (1<<TWINT) | (1<<TWEN);
	while (!(TWCR & (1<<TWINT)));
	return 0;
}

static uint8_t i2c_read_ack(void)
{
	TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
	while (!(TWCR & (1<<TWINT)));
	return TWDR;
}

static uint8_t i2c_read_nack(void)
{
	TWCR = (1<<TWINT) | (1<<TWEN);
	while (!(TWCR & (1<<TWINT)));
	return TWDR;
}

void twi_init_50k(void)
{
	// Em 1 MHz: SCL máx teórico é 62,5 kHz.
	// Prescaler=1, TWBR=2 => ~50 kHz
	TWSR = 0x00;
	TWBR = 2;
	TWCR = (1<<TWEN);
}

uint8_t i2c_write_reg(uint8_t dev7, uint8_t reg, uint8_t val)
{
	if (i2c_start((dev7<<1) | 0)) { i2c_stop(); return 1; }
	i2c_write(reg);
	i2c_write(val);
	i2c_stop();
	return 0;
}

uint8_t i2c_read_regs(uint8_t dev7, uint8_t reg, uint8_t *buf, uint8_t len)
{
	if (i2c_start((dev7<<1) | 0)) { i2c_stop(); return 1; }
	i2c_write(reg);

	if (i2c_start((dev7<<1) | 1)) { i2c_stop(); return 2; }

	for (uint8_t i = 0; i < len; i++)
	buf[i] = (i == (len - 1)) ? i2c_read_nack() : i2c_read_ack();

	i2c_stop();
	return 0;
}

uint8_t i2c_write_byte(uint8_t dev7, uint8_t val)
{
	if (i2c_start((dev7<<1) | 0)) { i2c_stop(); return 1; }
	i2c_write(val);
	i2c_stop();
	return 0;
}
