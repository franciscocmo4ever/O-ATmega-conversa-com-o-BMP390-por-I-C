/*
 * bmp390.h
 *
 * Driver mínimo do BMP390 para leitura de pressão (Pa) e temperatura (°C)
 * via I2C (endereço 7-bit: 0x76 ou 0x77)
 *
 * Referência: BMP390 datasheet (Appendix: Computation formulae reference implementation)
 *
 * Author: Francisco (projeto hPa) + ChatGPT
 */

#pragma once
#include <stdint.h>

typedef struct
{
    // coeficientes já convertidos (float/double)
    double par_t1, par_t2, par_t3;
    double par_p1, par_p2, par_p3, par_p4, par_p5, par_p6, par_p7, par_p8, par_p9, par_p10, par_p11;

    // t_lin (carregado pelo cálculo de temperatura e usado no de pressão)
    double t_lin;
} bmp390_calib_t;

typedef struct
{
    uint8_t addr;          // 7-bit
    bmp390_calib_t calib;
} bmp390_t;

// retorna 0 se ok; !=0 se erro (chip id / i2c / etc)
uint8_t bmp390_init(bmp390_t *dev, uint8_t addr7);

// retorna 0 se ok; !=0 se erro
uint8_t bmp390_read(bmp390_t *dev, double *temp_c, double *press_pa);
