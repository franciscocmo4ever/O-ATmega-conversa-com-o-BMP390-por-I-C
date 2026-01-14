/*
 * bmp390.h
 *
 * Created: 11/01/2026 21:01:21
 *  Author: franc
 */ 


#define F_CPU 1000000UL


#include <util/delay.h>
#include "bmp390.h"
#include "twi_hw.h"

// Registradores principais
#define BMP390_REG_CHIP_ID     0x00
#define BMP390_CHIP_ID_VAL     0x60   //

#define BMP390_REG_STATUS      0x03   // drdy bits (press/temp) :contentReference[oaicite:12]{index=12}
#define BMP390_REG_PRESS_XLSB  0x04   // 0x04..0x06 :contentReference[oaicite:13]{index=13}
#define BMP390_REG_TEMP_XLSB   0x07   // 0x07..0x09 :contentReference[oaicite:14]{index=14}

#define BMP390_REG_PWR_CTRL    0x1B
#define BMP390_REG_OSR         0x1C

// Calib NVM: 0x31..0x45 :contentReference[oaicite:15]{index=15}
#define BMP390_REG_CALIB_START 0x31
#define BMP390_CALIB_LEN       (0x45 - 0x31 + 1)

static double t_lin = 0.0;

// Datasheet Appendix (compensação) :contentReference[oaicite:16]{index=16}
static double bmp390_compensate_temperature(uint32_t uncomp_temp, const bmp390_calib_t *c)
{
	double partial_data1 = (double)uncomp_temp - c->par_t1;
	double partial_data2 = partial_data1 * c->par_t2;

	t_lin = partial_data2 + (partial_data1 * partial_data1) * c->par_t3;
	return t_lin;
}

static double bmp390_compensate_pressure(uint32_t uncomp_press, const bmp390_calib_t *c)
{
	double partial_data1 = c->par_p6 * t_lin;
	double partial_data2 = c->par_p7 * t_lin * t_lin;
	double partial_data3 = c->par_p8 * t_lin * t_lin * t_lin;
	double partial_out1  = c->par_p5 + partial_data1 + partial_data2 + partial_data3;

	partial_data1 = c->par_p2 * t_lin;
	partial_data2 = c->par_p3 * t_lin * t_lin;
	partial_data3 = c->par_p4 * t_lin * t_lin * t_lin;
	double partial_out2 = (double)uncomp_press * (c->par_p1 + partial_data1 + partial_data2 + partial_data3);

	partial_data1 = (double)uncomp_press * (double)uncomp_press;
	partial_data2 = c->par_p9 + (c->par_p10 * t_lin);
	partial_data3 = partial_data1 * partial_data2;
	double partial_data4 = partial_data3 + ((double)uncomp_press * partial_data1) * c->par_p11;

	return partial_out1 + partial_out2 + partial_data4;
}

static void parse_calib(bmp390_calib_t *c, const uint8_t *b)
{
	// Montagem little-endian (LSB no endereço menor) :contentReference[oaicite:17]{index=17}
	uint16_t nvm_t1 = (uint16_t)b[0]  | ((uint16_t)b[1]  << 8); // 0x31..0x32
	uint16_t nvm_t2 = (uint16_t)b[2]  | ((uint16_t)b[3]  << 8); // 0x33..0x34
	int8_t   nvm_t3 = (int8_t)b[4];                             // 0x35

	int16_t  nvm_p1 = (int16_t)((uint16_t)b[5]  | ((uint16_t)b[6]  << 8)); // 0x36..0x37
	int16_t  nvm_p2 = (int16_t)((uint16_t)b[7]  | ((uint16_t)b[8]  << 8)); // 0x38..0x39
	int8_t   nvm_p3 = (int8_t)b[9];                                        // 0x3A
	int8_t   nvm_p4 = (int8_t)b[10];                                       // 0x3B
	uint16_t nvm_p5 = (uint16_t)b[11] | ((uint16_t)b[12] << 8);            // 0x3C..0x3D (unsigned)
	uint16_t nvm_p6 = (uint16_t)b[13] | ((uint16_t)b[14] << 8);            // 0x3E..0x3F (unsigned)
	int8_t   nvm_p7 = (int8_t)b[15];                                       // 0x40
	int8_t   nvm_p8 = (int8_t)b[16];                                       // 0x41
	int16_t  nvm_p9 = (int16_t)((uint16_t)b[17] | ((uint16_t)b[18] << 8)); // 0x42..0x43
	int8_t   nvm_p10= (int8_t)b[19];                                       // 0x44
	int8_t   nvm_p11= (int8_t)b[20];                                       // 0x45

	// Conversões do datasheet (sec. 8.4) :contentReference[oaicite:18]{index=18}
	// Observação: o datasheet escreve divisões por 2^(expoente), incluindo expoentes negativos.
	c->par_t1 = (double)nvm_t1 * 256.0;                          // / 2^-8  = *2^8
	c->par_t2 = (double)nvm_t2 / 1073741824.0;                   // / 2^30
	c->par_t3 = (double)nvm_t3 / 281474976710656.0;              // / 2^48

	c->par_p1 = (double)((int32_t)nvm_p1 - 16384) / 1048576.0;   // (p1-2^14)/2^20
	c->par_p2 = (double)((int32_t)nvm_p2 - 16384) / 536870912.0; // (p2-2^14)/2^29
	c->par_p3 = (double)nvm_p3 / 4294967296.0;                   // /2^32
	c->par_p4 = (double)nvm_p4 / 137438953472.0;                 // /2^37
	c->par_p5 = (double)nvm_p5 * 8.0;                            // /2^-3 = *8
	c->par_p6 = (double)nvm_p6 / 64.0;                           // /2^6
	c->par_p7 = (double)nvm_p7 / 256.0;                          // /2^8
	c->par_p8 = (double)nvm_p8 / 32768.0;                        // /2^15
	c->par_p9 = (double)nvm_p9 / 281474976710656.0;              // /2^48
	c->par_p10= (double)nvm_p10/ 281474976710656.0;              // /2^48
	c->par_p11= (double)nvm_p11/ 36893488147419103232.0;         // /2^65
}

static uint8_t bmp390_read_chip_id(uint8_t addr, uint8_t *id)
{
	return i2c_read_regs(addr, BMP390_REG_CHIP_ID, id, 1);
}

uint8_t bmp390_init(bmp390_t *dev, uint8_t addr)
{
	dev->addr = addr;

	uint8_t id = 0;
	if (bmp390_read_chip_id(addr, &id)) return 1;
	if (id != BMP390_CHIP_ID_VAL) return 2;

	// Lê calib 0x31..0x45 :contentReference[oaicite:19]{index=19}
	uint8_t buf[BMP390_CALIB_LEN];
	if (i2c_read_regs(addr, BMP390_REG_CALIB_START, buf, BMP390_CALIB_LEN)) return 3;
	parse_calib(&dev->calib, buf);

	// Oversampling:
	// osr_p (bits 2..0), osr_t (bits 5..3)
	// 0x00=x1, 0x01=x2, 0x02=x4, 0x03=x8...
	const uint8_t osr_p = 0x02; // x4
	const uint8_t osr_t = 0x01; // x2
	uint8_t osr = (uint8_t)((osr_t << 3) | (osr_p));
	if (i2c_write_reg(addr, BMP390_REG_OSR, osr)) return 4;

	// PWR_CTRL:
	// bit0 press_en, bit1 temp_en, mode[5:4] (00 sleep, 01 forced, 10 normal)
	// Deixa em sleep por padrão; no loop a gente dispara forced.
	uint8_t pwr_sleep = (1<<0) | (1<<1); // press/temp enable, mode=00
	if (i2c_write_reg(addr, BMP390_REG_PWR_CTRL, pwr_sleep)) return 5;

	return 0;
}

static uint8_t bmp390_trigger_forced(uint8_t addr)
{
	// press_en=1, temp_en=1, mode=01 (forced)
	uint8_t pwr_forced = (1<<0) | (1<<1) | (1<<4);
	return i2c_write_reg(addr, BMP390_REG_PWR_CTRL, pwr_forced);
}

uint8_t bmp390_read(bmp390_t *dev, double *temp_c, double *press_pa)
{
	if (bmp390_trigger_forced(dev->addr)) return 1;

	// Espera data ready (STATUS 0x03: drdy_press e drdy_temp) :contentReference[oaicite:23]{index=23}
	for (uint16_t t = 0; t < 600; t++)
	{
		uint8_t st = 0;
		if (i2c_read_regs(dev->addr, BMP390_REG_STATUS, &st, 1)) return 2;

		uint8_t drdy_press = (st & (1<<5)) != 0;
		uint8_t drdy_temp  = (st & (1<<6)) != 0;
		if (drdy_press && drdy_temp) break;

		_delay_ms(1);
		if (t == 599) return 3;
	}

	// Lê 6 bytes: press(3) + temp(3) :contentReference[oaicite:24]{index=24}
	uint8_t b[6];
	if (i2c_read_regs(dev->addr, BMP390_REG_PRESS_XLSB, b, 6)) return 4;

	uint32_t up = ((uint32_t)b[2] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[0];
	uint32_t ut = ((uint32_t)b[5] << 16) | ((uint32_t)b[4] << 8) | (uint32_t)b[3];

	*temp_c  = bmp390_compensate_temperature(ut, &dev->calib);
	*press_pa= bmp390_compensate_pressure(up, &dev->calib);
	return 0;
}
