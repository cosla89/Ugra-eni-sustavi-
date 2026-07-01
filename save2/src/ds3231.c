#include "../drivers/ds3231.h"

#include "i2c.h"

static uint8_t bcd_to_bin(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10) + (value & 0x0F));
}

uint8_t ds3231_init(uint8_t address)
{
    if (!i2c_start((address << 1) | 0)) {
        i2c_stop();
        return 0;
    }
    i2c_stop();
    return 1;
}

uint8_t ds3231_read_time(uint8_t address, ds3231_time_t *time)
{
    if (!i2c_start((address << 1) | 0)) {
        i2c_stop();
        return 0;
    }

    if (!i2c_write(0x00)) {
        i2c_stop();
        return 0;
    }

    if (!i2c_start((address << 1) | 1)) {
        i2c_stop();
        return 0;
    }

    uint8_t seconds = i2c_read_ack();
    uint8_t minutes = i2c_read_ack();
    uint8_t hours = i2c_read_ack();
    uint8_t day = i2c_read_ack();
    uint8_t date = i2c_read_ack();
    uint8_t month = i2c_read_ack();
    uint8_t year = i2c_read_nack();
    i2c_stop();

    time->seconds = bcd_to_bin(seconds & 0x7F);
    time->minutes = bcd_to_bin(minutes & 0x7F);
    time->hours = bcd_to_bin(hours & 0x3F);
    time->day = bcd_to_bin(day & 0x07);
    time->date = bcd_to_bin(date & 0x3F);
    time->month = bcd_to_bin(month & 0x1F);
    time->year = bcd_to_bin(year);
    return 1;
}
