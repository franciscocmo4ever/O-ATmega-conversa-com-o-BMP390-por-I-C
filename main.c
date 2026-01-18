/*
 * main.c
 * ATmega328P @ 1 MHz
 * LCD 20x4 via PCF8574 (I2C)
 * BMP390 via I2C (addr 0x76 quando SDO=GND)
 */

#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

#include "twi_hw.h"
#include "lcd_pcf8574.h"
#include "bmp390.h"

// =======================
// ENDEREÇOS I2C
// =======================
#define BMP390_ADDR   0x76     // SDO = GND -> 0x76
#define LCD_ADDR      0x27     // troque para 0x3F se o seu for esse

// =======================
// I2C mais lento (bom para 1 MHz)
// =======================
static void i2c_safe_init_1mhz(void)
{
    // TWI ~62kHz (seguro em 1 MHz)
    TWSR = 0x00;        // prescaler = 1
    TWBR = 4;           // ~62.5 kHz
    TWCR = (1 << TWEN); // enable TWI
}

static void lcd_print_line(lcd_t *lcd, uint8_t row, const char *s)
{
    char buf[21];
    // Preenche com espaços e limita a 20 colunas
    snprintf(buf, sizeof(buf), "%-20.20s", s);
    lcd_goto(lcd, row, 0);
    lcd_print(lcd, buf);
}

int main(void)
{
    // 1) I2C init (seguro para F_CPU=1MHz)
    i2c_safe_init_1mhz();

    // 2) LCD init (SUA LIB)
    lcd_t lcd;
    lcd_init_20x4(&lcd, LCD_ADDR);   // <-- FUNÇÃO CERTA DA SUA LIB
    lcd_clear(&lcd);

    lcd_print_line(&lcd, 0, "BMP390 + LCD I2C");
    lcd_print_line(&lcd, 1, "Init BMP390...");

    // 3) BMP390 init (0x76)
    bmp390_t bmp;
    uint8_t err = bmp390_init(&bmp, BMP390_ADDR);

    if (err != 0)
    {
        char e[21];
        snprintf(e, sizeof(e), "BMP390 ERR %u", err);
        lcd_print_line(&lcd, 2, e);

        // No teu driver: ERR 2 = chip_id != 0x60
        if (err == 2)
            lcd_print_line(&lcd, 3, "ID!=0x60 (I2C)");

        while (1) { _delay_ms(500); }
    }

    lcd_print_line(&lcd, 1, "OK! Lendo...");
    _delay_ms(800);

    while (1)
    {
        double tC = 0.0;
        double pPa = 0.0;

        uint8_t r = bmp390_read(&bmp, &tC, &pPa);

        if (r == 0)
        {
            // Converte para inteiros (sem printf de float)
            int16_t temp_c_x100 = (int16_t)(tC * 100.0);      // ex: 2534 = 25.34C
            int32_t press_hpa_x10 = (int32_t)((pPa / 100.0) * 10.0); // ex: 10092 = 1009.2 hPa

            char l0[21], l1[21], l2[21], l3[21];

            snprintf(l0, sizeof(l0), "Temp: %2d.%02d C",
            temp_c_x100 / 100,
            (temp_c_x100 < 0 ? -temp_c_x100 : temp_c_x100) % 100);

            snprintf(l1, sizeof(l1), "Press:%4ld.%1ld hPa",
            press_hpa_x10 / 10,
            (press_hpa_x10 < 0 ? -press_hpa_x10 : press_hpa_x10) % 10);

            snprintf(l2, sizeof(l2), "BMP:0x%02X LCD:0x%02X", BMP390_ADDR, LCD_ADDR);
            snprintf(l3, sizeof(l3), "SDO=GND CSB=VCC");

            lcd_print_line(&lcd, 0, l0);
            lcd_print_line(&lcd, 1, l1);
            lcd_print_line(&lcd, 2, l2);
            lcd_print_line(&lcd, 3, l3);
        }
        else
        {
            char e[21];
            snprintf(e, sizeof(e), "READ ERR %u", r);
            lcd_print_line(&lcd, 0, e);
            lcd_print_line(&lcd, 1, "Verifique I2C");
            lcd_print_line(&lcd, 2, "0x76/CSB/Pullup");
            lcd_print_line(&lcd, 3, "");
        }

        _delay_ms(1000);
    }
}
