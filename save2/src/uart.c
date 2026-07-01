#include "../include/uart.h"

#include <avr/io.h>

#define BAUD_RATE 9600UL
#define UBRR_VALUE ((F_CPU / 8UL / BAUD_RATE) - 1UL)

void uart_init(void)
{
    UCSR0A = (1 << U2X0);
    UBRR0H = (unsigned char)(UBRR_VALUE >> 8);
    UBRR0L = (unsigned char)UBRR_VALUE;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0))) {
    }
    UDR0 = c;
}

void uart_puts(const char *text)
{
    while (*text) {
        uart_putc(*text++);
    }
}

void uart_putu16(unsigned int value)
{
    char buffer[6];
    char index = 0;

    if (value == 0) {
        uart_putc('0');
        return;
    }

    while (value > 0 && index < 5) {
        buffer[index++] = '0' + (value % 10);
        value /= 10;
    }

    while (index > 0) {
        uart_putc(buffer[--index]);
    }
}

void uart_putu32(uint32_t value)
{
    char buffer[11];
    char index = 0;

    if (value == 0) {
        uart_putc('0');
        return;
    }

    while (value > 0 && index < 10) {
        buffer[index++] = (char)('0' + (value % 10UL));
        value /= 10UL;
    }

    while (index > 0) {
        uart_putc(buffer[--index]);
    }
}

void uart_puti32(int32_t value)
{
    if (value < 0) {
        uart_putc('-');
        value = -value;
    }
    uart_putu32((uint32_t)value);
}