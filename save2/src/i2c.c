#include "../include/i2c.h"

#include <avr/io.h>
#include <util/twi.h>

#define I2C_TIMEOUT 60000U

static uint8_t i2c_wait_twint(void)
{
    uint16_t timeout = I2C_TIMEOUT;
    while (!(TWCR & (1 << TWINT))) {
        if (--timeout == 0) {
            TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
            return 0;
        }
    }
    return 1;
}

void i2c_init(void)
{
    TWSR = 0x00;
    TWBR = 72;
    PORTC |= (1 << PC4) | (1 << PC5);
    TWCR = (1 << TWEN);
}

uint8_t i2c_start(uint8_t address_rw)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (!i2c_wait_twint()) {
        return 0;
    }

    TWDR = address_rw;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!i2c_wait_twint()) {
        return 0;
    }

    return (TWSR & 0xF8) == TW_MT_SLA_ACK || (TWSR & 0xF8) == TW_MR_SLA_ACK;
}

void i2c_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}

uint8_t i2c_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!i2c_wait_twint()) {
        return 0;
    }

    return (TWSR & 0xF8) == TW_MT_DATA_ACK;
}

uint8_t i2c_read_ack(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    if (!i2c_wait_twint()) {
        return 0;
    }
    return TWDR;
}

uint8_t i2c_read_nack(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!i2c_wait_twint()) {
        return 0;
    }
    return TWDR;
}
