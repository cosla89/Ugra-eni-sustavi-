#include "../include/spi.h"

#include <avr/io.h>

void spi_set_slow(void)
{
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
    SPSR = 0;
}

void spi_set_fast(void)
{
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
    SPSR = (1 << SPI2X);
}

void spi_init(void)
{
    DDRB |= (1 << PB2) | (1 << PB3) | (1 << PB5);
    DDRB &= ~(1 << PB4);
    PORTB |= (1 << PB2) | (1 << PB4);
    spi_set_slow();
}

uint8_t spi_transfer(uint8_t data)
{
    SPDR = data;
    while (!(SPSR & (1 << SPIF))) {
    }
    return SPDR;
}
