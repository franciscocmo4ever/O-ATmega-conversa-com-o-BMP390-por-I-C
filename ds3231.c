/*
 * ds3231.h
 *
 * Created: 11/01/2026 21:04:57
 *  Author: franc
 */ 

#define F_CPU 1000000UL


#include "ds3231.h"
#include "twi_hw.h"

static uint8_t bcd2bin(uint8_t v){ return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t bin2bcd(uint8_t v){ return (uint8_t)(((v / 10) << 4) | (v % 10)); }

uint8_t ds3231_read_time(rtc_time_t *t)
{
	uint8_t b[7];
	if (i2c_read_regs(DS3231_ADDR, 0x00, b, 7)) return 1;

	t->sec  = bcd2bin(b[0] & 0x7F);
	t->min  = bcd2bin(b[1] & 0x7F);

	// Hora: assumindo modo 24h (bit6=0). Se estiver em 12h, tem conversão extra.
	if (b[2] & 0x40) {
		// 12h mode (bit6=1) – conversão simples:
		uint8_t hh = bcd2bin(b[2] & 0x1F);
		uint8_t pm = (b[2] & 0x20) ? 1 : 0;
		t->hour = (hh % 12) + (pm ? 12 : 0);
		} else {
		t->hour = bcd2bin(b[2] & 0x3F);
	}

	t->day   = bcd2bin(b[3] & 0x07);
	t->date  = bcd2bin(b[4] & 0x3F);
	t->month = bcd2bin(b[5] & 0x1F);
	t->year  = (uint16_t)(2000 + bcd2bin(b[6])); // DS3231 guarda 00..99
	return 0;
}

uint8_t ds3231_set_time(const rtc_time_t *t)
{
	// Escreve 0x00..0x06 de uma vez (precisa função “write burst”,
	// então vamos escrever registrador por registrador (simples).
	if (i2c_write_reg(DS3231_ADDR, 0x00, bin2bcd(t->sec)))  return 1;
	if (i2c_write_reg(DS3231_ADDR, 0x01, bin2bcd(t->min)))  return 2;
	if (i2c_write_reg(DS3231_ADDR, 0x02, bin2bcd(t->hour))) return 3; // força 24h
	if (i2c_write_reg(DS3231_ADDR, 0x03, bin2bcd(t->day)))  return 4;
	if (i2c_write_reg(DS3231_ADDR, 0x04, bin2bcd(t->date))) return 5;
	if (i2c_write_reg(DS3231_ADDR, 0x05, bin2bcd(t->month)))return 6;
	if (i2c_write_reg(DS3231_ADDR, 0x06, bin2bcd((uint8_t)(t->year - 2000)))) return 7;
	return 0;
}
