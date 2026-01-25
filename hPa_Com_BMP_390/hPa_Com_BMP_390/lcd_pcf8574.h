/*
 * lcd_pcf8574.h
 *
 * Created: 11/01/2026 21:05:20
 *  Author: franc
 */ 




#pragma once
#include <stdint.h>

typedef struct {
	uint8_t addr;      // ex: 0x27 ou 0x3F
	uint8_t backlight; // 0 ou 1
} lcd_t;

void lcd_init_20x4(lcd_t *lcd, uint8_t addr);
void lcd_clear(lcd_t *lcd);
void lcd_goto(lcd_t *lcd, uint8_t row, uint8_t col);
void lcd_print(lcd_t *lcd, const char *s);

// printf simples (sem float). Escreve no LCD (buffer de 20 chars).
// Ex.: lcd_printf(&lcd, "ERR:%u", err);
void lcd_printf(lcd_t *lcd, const char *fmt, ...);
