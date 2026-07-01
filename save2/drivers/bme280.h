#pragma once

#include <stdint.h>

#define BME280_ADDR 0x76

typedef struct {
    int32_t temperature_x100;
    uint32_t pressure_pa;
    uint32_t humidity_x100;
} bme280_data_t;

uint8_t bme280_init(uint8_t address);
uint8_t bme280_read_id(uint8_t address, uint8_t *id);
uint8_t bme280_read_data(uint8_t address, bme280_data_t *data);
