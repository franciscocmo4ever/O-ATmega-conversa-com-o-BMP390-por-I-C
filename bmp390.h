/*
 * bmp390.h
 *
 * Created: 11/01/2026 21:01:11
 *  Author: franc
 */ 





#pragma once
#include <stdint.h>

typedef struct
{
	// Coefs já convertidos (float/double)
	double par_t1, par_t2, par_t3;
	double par_p1, par_p2, par_p3, par_p4, par_p5, par_p6, par_p7, par_p8, par_p9, par_p10, par_p11;
} bmp390_calib_t;

typedef struct
{
	uint8_t addr;          // 0x76 ou 0x77
	bmp390_calib_t calib;
} bmp390_t;

uint8_t bmp390_init(bmp390_t *dev, uint8_t addr);
uint8_t bmp390_read(bmp390_t *dev, double *temp_c, double *press_pa);
