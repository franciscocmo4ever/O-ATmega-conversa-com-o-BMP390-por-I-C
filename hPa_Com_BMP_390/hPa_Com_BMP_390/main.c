/*
===============================================================================
Projeto: hPa_Station_BMP390_ATmega328P + DS3231 + LCD 20x4
Autor: Francisco de Castro Menezes Ouriques
MCU: ATmega328P | Clock: 1 MHz (RC interno)
I2C: TWI hardware (twi_hw.c)
Sensor: BMP390 (Bosch)  | I2C addr = 0x76  (SDO=GND / CSB=VCC)
RTC: DS3231             | I2C addr = 0x68
Display: LCD 20x4 via PCF8574 | addr tipico = 0x27 (ou 0x3F)
Versao: v1.1.2 (3 telas + historico 4h)  // FIX: sem %* no sprintf (LCD agora mostra numeros)
===============================================================================
TELAS
1) Data + Hora (com segundos) + dia da semana
2) Pressao (hPa) + Temperatura (C)
3) Historico ultimas 4 horas (1 amostra por hora)
===============================================================================
*/

#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>   // itoa

#include "twi_hw.h"
#include "lcd_pcf8574.h"
#include "bmp390.h"
#include "ds3231.h"

// =======================
// ENDERECOS I2C
// =======================
#define BMP390_ADDR   0x76
#define LCD_ADDR      0x27

// =======================
// CONFIG
// =======================
#define TICK_MS            200
#define SCREEN_TIME_MS     5000     // tempo em cada tela
#define SENSOR_PERIOD_MS   10000    // leitura BMP390

// =======================
// Historico (4 amostras)
// =======================
typedef struct {
    uint8_t hour;   // hora da amostra (0..23)
    int16_t p10;    // pressao *10 (hPa)
    int16_t t10;    // temp *10 (C)
    uint8_t valid;
} hist_t;

static hist_t  hist[4];
static uint8_t hist_head = 0;          // proxima posicao pra gravar
static uint8_t last_logged_hour = 255; // 255 = nao inicializado
static uint8_t live_valid = 0;
    uint8_t bmp_err = 0;

// =======================
// Util: dia da semana
// =======================
static const char* dow_str(uint8_t d)
{
    // DS3231: 1..7 (muitos projetos usam 1=DOM)
    switch(d){
        case 1: return "DOM";
        case 2: return "SEG";
        case 3: return "TER";
        case 4: return "QUA";
        case 5: return "QUI";
        case 6: return "SEX";
        case 7: return "SAB";
        default: return "---";
    }
}

// =======================
// Util: formata x10 SEM sprintf com largura dinamica
// (porque alguns toolchains/printf_min nao suportam "%*d" direito)
// Saida exemplo (pressao): " 1013.2"  (7 chars com espaco inicial)
// Saida exemplo (temp):    "  25.1"   (6 chars com espaco inicial)
// =======================
static void fmt_x10(char *out, int16_t v10, uint8_t width_int)
{
    uint8_t neg = 0;
    uint16_t a;

    if (v10 < 0) { neg = 1; a = (uint16_t)(-v10); }
    else         { a = (uint16_t)( v10); }

    uint16_t ip = (uint16_t)(a / 10);
    uint8_t  fp = (uint8_t)(a % 10);

    char tmp[8];
    itoa((int)ip, tmp, 10);

    uint8_t len = (uint8_t)strlen(tmp);
    uint8_t pad = 0;
    if (width_int > len) pad = (uint8_t)(width_int - len);

    char *p = out;
    *p++ = neg ? '-' : ' ';   // mantem 1 char para sinal/alinhamento

    while (pad--) *p++ = ' ';

    memcpy(p, tmp, len);
    p += len;

    *p++ = '.';
    *p++ = (char)('0' + fp);
    *p = 0;
}

// =======================
// Historico: grava a cada troca de hora
// =======================
static void hist_log_if_new_hour(uint8_t hour, int16_t p10, int16_t t10)
{
    if (last_logged_hour != 255 && hour == last_logged_hour) {
        return; // mesma hora
    }

    last_logged_hour = hour;

    hist[hist_head].hour  = hour;
    hist[hist_head].p10   = p10;
    hist[hist_head].t10   = t10;
    hist[hist_head].valid = 1;

    hist_head = (uint8_t)((hist_head + 1) & 0x03);
}

// =======================
// Tela 1: RTC
// =======================
static void screen_rtc(lcd_t *lcd, const rtc_time_t *t)
{
    char l1[21], l2[21];

    // Linha 1: "SEX 24/01/2026"
    snprintf(l1, sizeof(l1), "%s %02u/%02u/%04u",
             dow_str(t->day), t->date, t->month, t->year);

    // Linha 2: "Hora 23:54" (sem segundos)
    snprintf(l2, sizeof(l2), "Hora %02u:%02u",
             t->hour, t->min);

    lcd_clear(lcd);
    lcd_goto(lcd, 0, 0); lcd_print(lcd, l1);
    lcd_goto(lcd, 1, 0); lcd_print(lcd, l2);
    lcd_goto(lcd, 3, 0); lcd_print(lcd, "Tela 1/3 (RTC)      ");
}

// =======================
// Tela 2: Pressao + Temp
// =======================
static void screen_live(lcd_t *lcd, int16_t p10, int16_t t10, uint8_t ok, uint8_t err)
{
    lcd_clear(lcd);

    if (!ok) {
        lcd_goto(lcd, 0, 0); lcd_print(lcd, "hPa: ----.-         ");
        lcd_goto(lcd, 1, 0); lcd_printf(lcd, "BMP ERR:%3u          ", (unsigned)err);
        lcd_goto(lcd, 2, 0); lcd_print(lcd, "Temp: --.- C        ");
        lcd_goto(lcd, 3, 0); lcd_print(lcd, "Tela 2/3 (LIVE)     ");
        return;
    }

    char l1[21], l2[21], pbuf[12], tbuf[12];

    fmt_x10(pbuf, p10, 4); // " 1013.2"
    fmt_x10(tbuf, t10, 2); // "  25.1"

    // Linha 1: "hPa: 1013.2"
    snprintf(l1, sizeof(l1), "hPa:%s", pbuf);
    // Linha 2: "Temp: 25.1 C"
    snprintf(l2, sizeof(l2), "Temp:%s C", tbuf);

    lcd_goto(lcd, 0, 0); lcd_print(lcd, l1);
    lcd_goto(lcd, 1, 0); lcd_print(lcd, l2);
    lcd_goto(lcd, 3, 0); lcd_print(lcd, "Tela 2/3 (LIVE)     ");
}

// =======================
// Tela 3: Historico 4h
// =======================
static void screen_hist(lcd_t *lcd)
{
    lcd_clear(lcd);
    lcd_goto(lcd, 0, 0); lcd_print(lcd, "HIST (ultimas 4h)   ");

    // Mostra 3 linhas (1..3). A 4a amostra existe, mas nao cabe no 20x4 junto do titulo.
    // (Se quiser, a gente faz paginacao do historico depois.)
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t idx = (uint8_t)((hist_head + 3 - i) & 0x03); // mais recente primeiro
        char line[21];

        if (!hist[idx].valid) {
            snprintf(line, sizeof(line), "--h ----.- --.-     ");
        } else {
            char pbuf[12], tbuf[12];
            fmt_x10(pbuf, hist[idx].p10, 4);
            fmt_x10(tbuf, hist[idx].t10, 2);

            // remove espaco inicial (economiza 1 coluna)
            const char *pp = (pbuf[0] == ' ') ? (pbuf + 1) : pbuf;
            const char *tt = (tbuf[0] == ' ') ? (tbuf + 1) : tbuf;

            snprintf(line, sizeof(line), "%02uh %s %s        ",
                     hist[idx].hour, pp, tt);
        }

        lcd_goto(lcd, (uint8_t)(i + 1), 0);
        lcd_print(lcd, line);
    }

    // Rodape
    lcd_goto(lcd, 3, 0); lcd_print(lcd, "Tela 3/3 (HIST)     ");
}

int main(void)
{
    // =======================
    // Init I2C + perifericos
    // =======================
    i2c_safe_init_1mhz();

    lcd_t lcd;
    lcd_init_20x4(&lcd, LCD_ADDR);

    bmp390_t bmp;
    uint8_t err_bmp = bmp390_init(&bmp, BMP390_ADDR);

    // Se BMP falhar, avisa e trava (pra facilitar debug)
    if (err_bmp) {
        lcd_clear(&lcd);
        lcd_goto(&lcd, 0, 0); lcd_print(&lcd, "ERRO BMP390 init    ");
        char b[21];
        snprintf(b, sizeof(b), "err=%u addr=0x%02X   ", err_bmp, BMP390_ADDR);
        lcd_goto(&lcd, 1, 0); lcd_print(&lcd, b);
        lcd_goto(&lcd, 2, 0); lcd_print(&lcd, "Confirme 3V3 + I2C  ");
        while(1){ _delay_ms(500); }
    }

    // =======================
    // Loop
    // =======================
    uint8_t  screen = 0;
    uint16_t screen_timer = 0;
    uint16_t sensor_timer = 0;

    rtc_time_t now = {0};
    int16_t p10 = 0, t10 = 0;

    while (1)
    {
        // --- RTC ---
        (void)ds3231_read_time(&now);

        // --- Sensor a cada SENSOR_PERIOD_MS ---
        if (sensor_timer >= SENSOR_PERIOD_MS) {
            sensor_timer = 0;

            double tc, ppa;
            uint8_t e = bmp390_read(&bmp, &tc, &ppa);
            bmp_err = e;
            live_valid = (e == 0);

            if (e == 0) {
                // Pa -> hPa
                double phpa = ppa / 100.0;
                p10 = (int16_t)(phpa * 10.0);
                t10 = (int16_t)(tc * 10.0);
                
                // historico: grava quando muda a hora
                hist_log_if_new_hour(now.hour, p10, t10);
            }
        }

        // --- Render da tela atual ---
        if (screen == 0) {
            screen_rtc(&lcd, &now);
        } else if (screen == 1) {
            screen_live(&lcd, p10, t10, live_valid, bmp_err);
        } else {
            screen_hist(&lcd);
        }

        // --- Tick ---
        _delay_ms(TICK_MS);
        screen_timer = (uint16_t)(screen_timer + TICK_MS);
        sensor_timer = (uint16_t)(sensor_timer + TICK_MS);

        // --- Troca de tela ---
        if (screen_timer >= SCREEN_TIME_MS) {
            screen_timer = 0;
            screen = (uint8_t)((screen + 1) % 3);
        }
    }
}