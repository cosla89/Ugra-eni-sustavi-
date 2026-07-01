#pragma once

#include <stdint.h>

#define BH1750_ADDR 0x23

uint8_t bh1750_init(uint8_t address);
uint8_t bh1750_read_lux(uint8_t address, uint16_t *lux);
