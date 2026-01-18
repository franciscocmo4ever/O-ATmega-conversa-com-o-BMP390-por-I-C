/*
 * twi_hw.h
 *
 * Created: 11/01/2026 21:00:37
 *  Author: franc
 */ 




#pragma once
#include <stdint.h>

void twi_init_50k(void);

uint8_t i2c_write_reg(uint8_t dev7, uint8_t reg, uint8_t val);
uint8_t i2c_read_regs(uint8_t dev7, uint8_t reg, uint8_t *buf, uint8_t len);

// Para PCF8574 (não tem registradores, é “write byte”)
uint8_t i2c_write_byte(uint8_t dev7, uint8_t val);
