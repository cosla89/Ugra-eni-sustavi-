#pragma once

#include <stdint.h>

#define DS3231_ADDR 0x68

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
} ds3231_time_t;

uint8_t ds3231_init(uint8_t address);
uint8_t ds3231_read_time(uint8_t address, ds3231_time_t *time);
