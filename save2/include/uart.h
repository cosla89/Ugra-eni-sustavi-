#pragma once

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *text);
void uart_putu16(unsigned int value);
void uart_putu32(uint32_t value);
void uart_puti32(int32_t value);
