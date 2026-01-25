/*
 * ds3231.h
 *
 * Created: 11/01/2026 21:04:46
 *  Author: franc
 */ 



#pragma once
#include <stdint.h>

typedef struct {
	uint8_t sec, min, hour;
	uint8_t day;   // 1..7 (opcional)
	uint8_t date;  // 1..31
	uint8_t month; // 1..12
	uint16_t year; // ex: 2026
} rtc_time_t;

#define DS3231_ADDR 0x68

uint8_t ds3231_read_time(rtc_time_t *t);
uint8_t ds3231_set_time(const rtc_time_t *t);
