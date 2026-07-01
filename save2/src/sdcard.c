#include "../drivers/sdcard.h"
#include "../include/spi.h"

#include <avr/io.h>
#include <util/delay.h>

#define SD_CS_LOW()  (PORTB &= ~(1 << PB2))
#define SD_CS_HIGH() (PORTB |= (1 << PB2))

static uint8_t high_capacity_card = 0;

static uint8_t sdcard_command(uint8_t cmd, uint32_t arg, uint8_t crc);
static void sdcard_end_cmd(void);

static void sdcard_clock_idle(uint8_t cycles)
{
    SD_CS_HIGH();
    while (cycles--) {
        spi_transfer(0xFF);
    }
}

static uint8_t sdcard_command(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t response;

    SD_CS_HIGH();
    spi_transfer(0xFF);
    SD_CS_LOW();

    spi_transfer((uint8_t)(0x40 | cmd));
    spi_transfer((uint8_t)(arg >> 24));
    spi_transfer((uint8_t)(arg >> 16));
    spi_transfer((uint8_t)(arg >> 8));
    spi_transfer((uint8_t)arg);
    spi_transfer(crc);

    for (uint8_t i = 0; i < 10; i++) {
        response = spi_transfer(0xFF);
        if (!(response & 0x80)) {
            return response;
        }
    }

    SD_CS_HIGH();
    spi_transfer(0xFF);
    return 0xFF;
}

static void sdcard_end_cmd(void)
{
    SD_CS_HIGH();
    spi_transfer(0xFF);
}

sdcard_err_t sdcard_init_debug(void)
{
    uint8_t response = 0xFF;
    uint8_t ocr[4];
    uint8_t r55;

    spi_init();
    spi_set_slow();
    _delay_ms(20);
    sdcard_clock_idle(80);

    for (uint8_t i = 0; i < 20; i++) {
        response = sdcard_command(0, 0, 0x95);
        sdcard_end_cmd();
        if (response == 0x01) break;
        _delay_ms(10);
    }
    if (response != 0x01) return SD_ERR_CMD0;

    response = sdcard_command(8, 0x000001AAUL, 0x87);
    if (response == 0x01) {
        for (uint8_t i = 0; i < 4; i++) {
            ocr[i] = spi_transfer(0xFF);
        }
        sdcard_end_cmd();

        if (ocr[2] != 0x01 || ocr[3] != 0xAA) {
            return SD_ERR_CMD8_VOLT;
        }
        high_capacity_card = 1;
    } else {
        sdcard_end_cmd();
        high_capacity_card = 0;
    }

    for (uint16_t i = 0; i < 500; i++) {
        SD_CS_HIGH();
        spi_transfer(0xFF);
        spi_transfer(0xFF);
        spi_transfer(0xFF);

        r55 = sdcard_command(55, 0, 0x01);
        sdcard_end_cmd();

        if (r55 != 0xFF) {
            response = sdcard_command(41, high_capacity_card ? 0x40000000UL : 0, 0x01);
            sdcard_end_cmd();
            if (response == 0x00) break;
        }

        _delay_ms(10);
    }
    if (response != 0x00) return SD_ERR_ACMD41;

    response = sdcard_command(58, 0, 0x01);
    if (response == 0x00) {
        for (uint8_t i = 0; i < 4; i++) {
            ocr[i] = spi_transfer(0xFF);
        }
        high_capacity_card = (ocr[0] & 0x40) ? 1 : 0;
    } else {
        sdcard_end_cmd();
        return SD_ERR_CMD58;
    }
    sdcard_end_cmd();

    response = sdcard_command(16, 512, 0x01);
    sdcard_end_cmd();
    if (response != 0x00 && !high_capacity_card) return SD_ERR_CMD16;

    spi_set_fast();
    return SD_ERR_NONE;
}

uint8_t sdcard_init(void)
{
    return (sdcard_init_debug() == SD_ERR_NONE) ? 1 : 0;
}

uint8_t sdcard_write_block(uint32_t block, const uint8_t *data)
{
    uint32_t address = high_capacity_card ? block : (block * 512UL);
    uint8_t response = sdcard_command(24, address, 0x01);

    if (response != 0x00) {
        sdcard_end_cmd();
        return 0;
    }

    spi_transfer(0xFE);
    for (uint16_t i = 0; i < 512; i++) {
        spi_transfer(data[i]);
    }
    spi_transfer(0xFF);
    spi_transfer(0xFF);

    response = spi_transfer(0xFF);
    if ((response & 0x1F) != 0x05) {
        sdcard_end_cmd();
        return 0;
    }

    uint16_t timeout = 0xFFFF;
    while (spi_transfer(0xFF) == 0x00) {
        if (--timeout == 0) {
            sdcard_end_cmd();
            return 0;
        }
    }

    sdcard_end_cmd();
    return 1;
}
