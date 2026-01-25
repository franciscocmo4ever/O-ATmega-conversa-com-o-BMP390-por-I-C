/*
 * bmp390.c
 *
 * Driver mínimo do BMP390 (I2C) para ATmega328P.
 * - init: softreset, chip_id, lê calibração, configura OSR + IIR, habilita sensores.
 * - read: dispara forced measurement, espera DRDY, lê dados (burst), compensa.
 *
 * Datasheet: BST-BMP390-DS002 (Appendix: computation formulae reference implementation)
 */

#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <util/delay.h>
#include "bmp390.h"
#include "twi_hw.h"

// ---------- registers ----------
#define BMP390_REG_CHIP_ID      0x00
#define BMP390_REG_ERR_REG      0x02
#define BMP390_REG_STATUS       0x03
#define BMP390_REG_DATA_0       0x04  // press xlsb
#define BMP390_REG_DATA_3       0x07  // temp xlsb
#define BMP390_REG_PWR_CTRL     0x1B
#define BMP390_REG_OSR          0x1C
#define BMP390_REG_ODR          0x1D
#define BMP390_REG_CONFIG       0x1F
#define BMP390_REG_CALIB_START  0x31
#define BMP390_REG_CMD          0x7E

// ---------- commands ----------
#define BMP390_CMD_SOFTRESET    0xB6

// ---------- status bits ----------
#define BMP390_STATUS_CMD_RDY   (1u << 4)
#define BMP390_STATUS_DRDY_P    (1u << 5)
#define BMP390_STATUS_DRDY_T    (1u << 6)

// ---------- helpers ----------
static uint16_t u16_le(const uint8_t *b) { return (uint16_t)((uint16_t)b[1] << 8 | b[0]); }
static int16_t  s16_le(const uint8_t *b) { return (int16_t)((int16_t)b[1] << 8 | b[0]); }

// coef conversions (do datasheet)
static void bmp390_parse_and_convert_calib(bmp390_calib_t *c, const uint8_t *d21)
{
    // Raw NVM values (Table 24)
    uint16_t nvm_t1 = u16_le(&d21[0]);
    uint16_t nvm_t2 = u16_le(&d21[2]);
    int8_t   nvm_t3 = (int8_t)d21[4];

    int16_t  nvm_p1 = s16_le(&d21[5]);
    int16_t  nvm_p2 = s16_le(&d21[7]);
    int8_t   nvm_p3 = (int8_t)d21[9];
    int8_t   nvm_p4 = (int8_t)d21[10];
    uint16_t nvm_p5 = u16_le(&d21[11]);
    uint16_t nvm_p6 = u16_le(&d21[13]);
    int8_t   nvm_p7 = (int8_t)d21[15];
    int8_t   nvm_p8 = (int8_t)d21[16];
    int16_t  nvm_p9 = s16_le(&d21[17]);
    int8_t   nvm_p10= (int8_t)d21[19];
    int8_t   nvm_p11= (int8_t)d21[20];

    // Conversions (Appendix 8.4)
    c->par_t1  = (double)nvm_t1 * 256.0;                 // /2^-8
    c->par_t2  = (double)nvm_t2 / 1073741824.0;          // /2^30
    c->par_t3  = (double)nvm_t3 / 281474976710656.0;     // /2^48

    c->par_p1  = ((double)nvm_p1 - 16384.0) / 1048576.0; // (nvm-2^14)/2^20
    c->par_p2  = ((double)nvm_p2 - 16384.0) / 536870912.0;// (nvm-2^14)/2^29
    c->par_p3  = (double)nvm_p3 / 4294967296.0;          // /2^32
    c->par_p4  = (double)nvm_p4 / 137438953472.0;        // /2^37
    c->par_p5  = (double)nvm_p5 * 8.0;                   // /2^-3
    c->par_p6  = (double)nvm_p6 / 64.0;                  // /2^6
    c->par_p7  = (double)nvm_p7 / 256.0;                 // /2^8
    c->par_p8  = (double)nvm_p8 / 32768.0;               // /2^15
    c->par_p9  = (double)nvm_p9 / 281474976710656.0;     // /2^48
    c->par_p10 = (double)nvm_p10 / 281474976710656.0;    // /2^48
    c->par_p11 = (double)nvm_p11 / 36893488147419103232.0;// /2^65

    c->t_lin = 0.0;
}

// Datasheet Appendix 8.5 (retorna t_lin; temperatura em °C = t_lin)
static double bmp390_comp_temp(uint32_t uncomp_temp, bmp390_calib_t *c)
{
    double partial_data1 = (double)uncomp_temp - c->par_t1;
    double partial_data2 = partial_data1 * c->par_t2;

    c->t_lin = partial_data2 + (partial_data1 * partial_data1) * c->par_t3;
    return c->t_lin;
}

// Datasheet Appendix 8.6 (retorna pressão em Pa)
static double bmp390_comp_press(uint32_t uncomp_press, bmp390_calib_t *c)
{
    double t = c->t_lin;

    double partial_data1 = c->par_p6 * t;
    double partial_data2 = c->par_p7 * (t * t);
    double partial_data3 = c->par_p8 * (t * t * t);
    double partial_out1  = c->par_p5 + partial_data1 + partial_data2 + partial_data3;

    partial_data1 = c->par_p2 * t;
    partial_data2 = c->par_p3 * (t * t);
    partial_data3 = c->par_p4 * (t * t * t);
    double partial_out2 = (double)uncomp_press * (c->par_p1 + partial_data1 + partial_data2 + partial_data3);

    partial_data1 = (double)uncomp_press * (double)uncomp_press;
    partial_data2 = c->par_p9 + c->par_p10 * t;
    partial_data3 = partial_data1 * partial_data2;

    double partial_data4 = partial_data3 + ((double)uncomp_press * (double)uncomp_press * (double)uncomp_press) * c->par_p11;

    return partial_out1 + partial_out2 + partial_data4;
}

uint8_t bmp390_init(bmp390_t *dev, uint8_t addr7)
{
    if (!dev) return 100;

    dev->addr = addr7;

    // soft reset
    if (i2c_write_reg(dev->addr, BMP390_REG_CMD, BMP390_CMD_SOFTRESET)) return 1;
    _delay_ms(5);

    // chip id
    uint8_t id = 0;
    if (i2c_read_reg(dev->addr, BMP390_REG_CHIP_ID, &id)) return 2;

    // Datasheet: chip_id novo (varia por revisão). Em muitos módulos aparece 0x60.
    if (id != 0x60 && id != 0x50) {
        // ainda assim deixa seguir, mas avisa via erro = 3
        return 3;
    }

    // lê calibração (0x31..0x45 => 21 bytes)
    uint8_t calib[21];
    if (i2c_read_regs(dev->addr, BMP390_REG_CALIB_START, calib, 21)) return 4;
    bmp390_parse_and_convert_calib(&dev->calib, calib);

    // Configura OSR (press x4, temp x2): osr_t em bits 5..3, osr_p em 2..0
    // osr_p: 010=x4 ; osr_t: 001=x2  => 0b001_010 = 0x0A
    if (i2c_write_reg(dev->addr, BMP390_REG_OSR, 0x0A)) return 5;

    // IIR filter coef_3 (010 em bits 3..1) => 0x04
    if (i2c_write_reg(dev->addr, BMP390_REG_CONFIG, 0x04)) return 6;

    // ODR não importa em forced, mas deixa default 0x00
    (void)i2c_write_reg(dev->addr, BMP390_REG_ODR, 0x00);

    // habilita sensores e deixa em sleep (mode=00)
    // press_en=1 (bit0), temp_en=1 (bit1), mode=00 (bits 5..4)
    if (i2c_write_reg(dev->addr, BMP390_REG_PWR_CTRL, 0x03)) return 7;

    return 0;
}

uint8_t bmp390_read(bmp390_t *dev, double *temp_c, double *press_pa)
{
    if (!dev || !temp_c || !press_pa) return 100;

    // Dispara forced measurement: mode=01 em bits 5..4 => 0x10; + press_en/temp_en => 0x03
    if (i2c_write_reg(dev->addr, BMP390_REG_PWR_CTRL, 0x13)) return 1;

    // espera DRDY (temp e press)
    for (uint16_t t = 0; t < 800; t++) {
        uint8_t st = 0;
        if (i2c_read_reg(dev->addr, BMP390_REG_STATUS, &st)) return 2;

        if ((st & BMP390_STATUS_DRDY_P) && (st & BMP390_STATUS_DRDY_T)) break;

        _delay_ms(1);
        if (t == 799) return 3;
    }

    // burst read: press(3) + temp(3), little-endian (xlsb,lsb,msb)
    uint8_t b[6];
    if (i2c_read_regs(dev->addr, BMP390_REG_DATA_0, b, 6)) return 4;

    uint32_t up = ((uint32_t)b[2] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[0];
    uint32_t ut = ((uint32_t)b[5] << 16) | ((uint32_t)b[4] << 8) | (uint32_t)b[3];

    double tlin = bmp390_comp_temp(ut, &dev->calib);
    double pa   = bmp390_comp_press(up, &dev->calib);

    *temp_c  = tlin;  // datasheet: t_lin já é temperatura compensada (°C)
    *press_pa= pa;

    return 0;
}
