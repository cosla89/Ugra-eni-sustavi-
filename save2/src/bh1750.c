#include "../drivers/bh1750.h"

#include "i2c.h"

#include <util/delay.h>

uint8_t bh1750_init(uint8_t address)
{
    if (!i2c_start((address << 1) | 0)) {
        i2c_stop();
        return 0;
    }

    if (!i2c_write(0x01)) {
        i2c_stop();
        return 0;
    }

    i2c_stop();
    return 1;
}

uint8_t bh1750_read_lux(uint8_t address, uint16_t *lux)
{
    if (!i2c_start((address << 1) | 0)) {
        i2c_stop();
        return 0;
    }

    if (!i2c_write(0x10)) {
        i2c_stop();
        return 0;
    }

    i2c_stop();
    _delay_ms(180);

    if (!i2c_start((address << 1) | 1)) {
        i2c_stop();
        return 0;
    }

    uint8_t msb = i2c_read_ack();
    uint8_t lsb = i2c_read_nack();
    i2c_stop();

    uint16_t raw = (uint16_t)(((uint16_t)msb << 8) | lsb);
    *lux = (uint16_t)(((uint32_t)raw * 5UL) / 6UL);
    return 1;
}
