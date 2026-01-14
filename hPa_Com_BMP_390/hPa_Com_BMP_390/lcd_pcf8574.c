/*
 * lcd_pcf8574.h
 *
 * Created: 11/01/2026 21:05:28
 *  Author: franc
 */ 




#define F_CPU 1000000UL
#include <util/delay.h>
#include "lcd_pcf8574.h"
#include "twi_hw.h"

// ====== Ajuste aqui se teu backpack for diferente ======
#define LCD_PCF_RS   (1<<0)
#define LCD_PCF_RW   (1<<1)
#define LCD_PCF_E    (1<<2)
#define LCD_PCF_BL   (1<<3)
#define LCD_PCF_D4   (1<<4)
#define LCD_PCF_D5   (1<<5)
#define LCD_PCF_D6   (1<<6)
#define LCD_PCF_D7   (1<<7)
// =======================================================

static uint8_t g_bl = LCD_PCF_BL;

static uint8_t pack_nibble(uint8_t nib, uint8_t rs)
{
	uint8_t v = 0;
	if (rs) v |= LCD_PCF_RS;
	if (g_bl) v |= LCD_PCF_BL;

	if (nib & 0x01) v |= LCD_PCF_D4;
	if (nib & 0x02) v |= LCD_PCF_D5;
	if (nib & 0x04) v |= LCD_PCF_D6;
	if (nib & 0x08) v |= LCD_PCF_D7;

	return v;
}

static void pcf_write(lcd_t *lcd, uint8_t data)
{
	i2c_write_byte(lcd->addr, data);
}

static void pulse_e(lcd_t *lcd, uint8_t data)
{
	pcf_write(lcd, data | LCD_PCF_E);
	_delay_us(2);
	pcf_write(lcd, data & (uint8_t)~LCD_PCF_E);
	_delay_us(50);
}

static void send4(lcd_t *lcd, uint8_t nib, uint8_t rs)
{
	uint8_t d = pack_nibble(nib, rs);
	pulse_e(lcd, d);
}

static void send8(lcd_t *lcd, uint8_t val, uint8_t rs)
{
	send4(lcd, (uint8_t)(val >> 4), rs);
	send4(lcd, (uint8_t)(val & 0x0F), rs);
}

static void cmd(lcd_t *lcd, uint8_t c){ send8(lcd, c, 0); }
static void dat(lcd_t *lcd, uint8_t c){ send8(lcd, c, 1); }

void lcd_init_20x4(lcd_t *lcd, uint8_t addr)
{
	lcd->addr = addr;
	lcd->backlight = 1;
	g_bl = LCD_PCF_BL;

	_delay_ms(50);

	// Sequência padrão HD44780 pra entrar em 4-bit via I2C backpack
	send4(lcd, 0x03, 0); _delay_ms(5);
	send4(lcd, 0x03, 0); _delay_ms(5);
	send4(lcd, 0x03, 0); _delay_ms(1);
	send4(lcd, 0x02, 0); _delay_ms(1); // 4-bit

	cmd(lcd, 0x28); // 4-bit, 2 linhas (controlador trata 20x4 como 2x40)
	cmd(lcd, 0x0C); // display on, cursor off
	cmd(lcd, 0x06); // entry mode
	cmd(lcd, 0x01); _delay_ms(2); // clear
}

void lcd_clear(lcd_t *lcd)
{
	cmd(lcd, 0x01);
	_delay_ms(2);
}

void lcd_goto(lcd_t *lcd, uint8_t row, uint8_t col)
{
	// offsets típicos do 20x4:
	// row0=0x00, row1=0x40, row2=0x14, row3=0x54
	static const uint8_t off[4] = {0x00, 0x40, 0x14, 0x54};
	if (row > 3) row = 3;
	cmd(lcd, (uint8_t)(0x80 | (off[row] + col)));
}

void lcd_print(lcd_t *lcd, const char *s)
{
	while (*s) dat(lcd, (uint8_t)*s++);
}
