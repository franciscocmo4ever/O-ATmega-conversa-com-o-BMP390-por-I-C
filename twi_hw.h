/*
 * twi_hw.h
 *
 * Created: 11/01/2026 21:00:37
 *  Author: franc
 */ 




#pragma once
#include <stdint.h>

void i2c_write_byte(uint8_t addr7, uint8_t data);


// Inicializa o TWI (I2C) em modo mais "seguro" pra F_CPU=1MHz
void i2c_safe_init_1mhz(void);

// Escrita/leitura gen�ricas (endere�o 7-bit: ex 0x76, 0x27)
uint8_t i2c_write_reg(uint8_t dev7, uint8_t reg, uint8_t val);
uint8_t i2c_read_reg(uint8_t dev7, uint8_t reg, uint8_t *val);
uint8_t i2c_read_regs(uint8_t dev7, uint8_t reg, uint8_t *buf, uint8_t len);
