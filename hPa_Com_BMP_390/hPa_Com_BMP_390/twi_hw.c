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


// NOTE:
// O PCF8574 recebe 1 byte "cru" (não tem registrador interno).
// Esta função precisa usar as primitivas TWI deste arquivo.
// (Versões antigas chamavam twi_start/twi_write/twi_stop, que não existem aqui.)
// A implementação real está mais abaixo, depois das primitivas.


// --------- timeouts para n�o travar no while(TWINT==0) ----------
#define I2C_TIMEOUT_LOOPS  40000UL

static uint8_t twi_wait_twint(void)
{
	uint32_t n = I2C_TIMEOUT_LOOPS;
	while (!(TWCR & (1 << TWINT)))
	{
		if (--n == 0) return 1; // timeout
	}
	return 0;
}

static inline uint8_t twi_status(void)
{
	// Status = TWSR & 0xF8 (datasheet AVR)
	return (uint8_t)(TWSR & 0xF8);
}

// ----------------- INIT -----------------
void i2c_safe_init_1mhz(void)
{
	// SCL = F_CPU / (16 + 2*TWBR*prescaler)
	// com F_CPU=1MHz, prescaler=1:
	// TWBR=4 => ~62.5kHz
	TWSR = 0x00;        // prescaler = 1
	TWBR = 4;           // ~62.5 kHz (seguro pra 1MHz)
	TWCR = (1 << TWEN); // enable TWI
}

// ----------------- PRIMITIVAS -----------------
static uint8_t i2c_start(uint8_t address_rw)
{
	// Gera condi��o START
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

	// Espera START completar
	if (twi_wait_twint()) return 1;

	// Confere status START enviado (0x08) ou repeated START (0x10)
	uint8_t st = twi_status();
	if (st != 0x08 && st != 0x10) return 2;

	// Carrega SLA+R/W no registrador de dados
	TWDR = address_rw;

	// Envia SLA+R/W
	TWCR = (1 << TWINT) | (1 << TWEN);

	if (twi_wait_twint()) return 3;

	// Checa ACK do escravo
	st = twi_status();
	// SLA+W ACK = 0x18, SLA+R ACK = 0x40
	if (st != 0x18 && st != 0x40) return 4;

	return 0; // ok
}

static void i2c_stop(void)
{
	// Gera STOP
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
	_delay_us(10);
}

static uint8_t i2c_write(uint8_t data)
{
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);

	if (twi_wait_twint()) return 1;

	// DATA ACK = 0x28
	if (twi_status() != 0x28) return 2;

	return 0;
}

static uint8_t i2c_read_ack(uint8_t *data)
{
	// l� e responde ACK (para continuar lendo mais bytes)
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);

	if (twi_wait_twint()) return 1;

	*data = TWDR;
	return 0;
}

static uint8_t i2c_read_nack(uint8_t *data)
{
	// l� e responde NACK (�ltimo byte)
	TWCR = (1 << TWINT) | (1 << TWEN);

	if (twi_wait_twint()) return 1;

	*data = TWDR;
	return 0;
}

// ----------------- FUN��ES "ALTAS" -----------------
uint8_t i2c_write_reg(uint8_t dev7, uint8_t reg, uint8_t val)
{
	// dev7 � 7-bit (ex: 0x76). Aqui montamos SLA+W:
	// (dev7<<1) | 0
	if (i2c_start((dev7 << 1) | 0)) { i2c_stop(); return 1; }
	if (i2c_write(reg))              { i2c_stop(); return 2; }
	if (i2c_write(val))              { i2c_stop(); return 3; }
	i2c_stop();
	return 0;
}

uint8_t i2c_read_reg(uint8_t dev7, uint8_t reg, uint8_t *val)
{
	if (i2c_start((dev7 << 1) | 0)) { i2c_stop(); return 1; }
	if (i2c_write(reg))             { i2c_stop(); return 2; }

	// Repeated start em modo leitura
	if (i2c_start((dev7 << 1) | 1)) { i2c_stop(); return 3; }

	if (i2c_read_nack(val))         { i2c_stop(); return 4; }
	i2c_stop();
	return 0;
}

uint8_t i2c_read_regs(uint8_t dev7, uint8_t reg, uint8_t *buf, uint8_t len)
{
	if (len == 0) return 0;

	if (i2c_start((dev7 << 1) | 0)) { i2c_stop(); return 1; }
	if (i2c_write(reg))             { i2c_stop(); return 2; }

	if (i2c_start((dev7 << 1) | 1)) { i2c_stop(); return 3; }

	for (uint8_t i = 0; i < len; i++)
	{
		if (i == (len - 1))
		{
			if (i2c_read_nack(&buf[i])) { i2c_stop(); return 4; }
		}
		else
		{
			if (i2c_read_ack(&buf[i]))  { i2c_stop(); return 5; }
		}
	}

	i2c_stop();
	return 0;
}

// ----------------- ESCRITA "CRUA" (PCF8574) -----------------
void i2c_write_byte(uint8_t addr7, uint8_t data)
{
    // addr7 é 7-bit (ex.: 0x27). Monta SLA+W aqui.
    if (i2c_start((addr7 << 1) | 0)) { i2c_stop(); return; }
    (void)i2c_write(data);
    i2c_stop();
}

