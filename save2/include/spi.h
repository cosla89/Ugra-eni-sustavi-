#pragma once

#include <stdint.h>

void spi_init(void);
void spi_set_slow(void);
void spi_set_fast(void);
uint8_t spi_transfer(uint8_t data);
