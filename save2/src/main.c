#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>

#include "../drivers/bme280.h"
#include "../drivers/bh1750.h"
#include "../drivers/ds3231.h"
#include "../drivers/sdcard.h"
#include "../include/i2c.h"
#include "../include/uart.h"

#define LOG_INTERVAL_MS          10000UL
#define PRESSURE_TREND_THRESHOLD 10UL

static uint32_t next_log_block     = SDCARD_LOG_START_BLOCK;
static uint32_t previous_pressure  = 0;
static uint8_t  have_prev_pressure = 0;
static uint32_t uptime_seconds     = 0;
static uint8_t  bme_addr           = 0x77;

/* Tipovi trenda pritiska, izdvojeni direktno ovdje jer više nema OLED drivera */
typedef enum {
    TREND_STABLE = 0,
    TREND_UP     = 1,
    TREND_DOWN   = 2
} pressure_trend_t;

static void wait_log_interval(void)
{
    for (uint16_t i = 0; i < (LOG_INTERVAL_MS / 100UL); i++) _delay_ms(100);
    uptime_seconds += LOG_INTERVAL_MS / 1000UL;
}

static pressure_trend_t calculate_pressure_trend(uint32_t p)
{
    pressure_trend_t trend = TREND_STABLE;
    if (have_prev_pressure) {
        if (p > previous_pressure + PRESSURE_TREND_THRESHOLD) trend = TREND_UP;
        else if (previous_pressure > p + PRESSURE_TREND_THRESHOLD) trend = TREND_DOWN;
    }
    previous_pressure  = p;
    have_prev_pressure = 1;
    return trend;
}

/* Pomoćne funkcije za brzo kreiranje CSV stringa bez teških biblioteka */
static void append_char(char *buf, uint16_t *pos, char c)
{
    buf[(*pos)++] = c;
    buf[*pos] = '\0';
}

static void append_text(char *buf, uint16_t *pos, const char *t)
{
    while (*t) buf[(*pos)++] = *t++;
    buf[*pos] = '\0';
}

static void append_u32(char *buf, uint16_t *pos, uint32_t val)
{
    char tmp[11];
    uint8_t i = 0;
    if (val == 0) {
        append_char(buf, pos, '0');
        return;
    }
    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) append_char(buf, pos, tmp[--i]);
}

static void append_fixed_x100(char *buf, uint16_t *pos, int32_t val)
{
    if (val < 0) {
        append_char(buf, pos, '-');
        val = -val;
    }
    append_u32(buf, pos, val / 100);
    append_char(buf, pos, '.');
    uint32_t rem = val % 100;
    if (rem < 10) append_char(buf, pos, '0');
    append_u32(buf, pos, rem);
}

/* Funkcija koja skuplja podatke i zapisuje ih u sirovi sektor SD kartice */
static uint8_t log_record(const ds3231_time_t *time, uint8_t rtc_ok, const bme280_data_t *bme, uint16_t lux, pressure_trend_t trend)
{
    static uint8_t sector_buffer[512];
    static uint16_t buffer_pos = 0;
    
    char row[128];
    uint16_t row_pos = 0;

    // Ako počinjemo od prvog bloka i buffer je prazan, upiši CSV zaglavlje
    if (next_log_block == SDCARD_LOG_START_BLOCK && buffer_pos == 0) {
        append_text(row, &row_pos, "timestamp,temp_c,pressure_pa,lux,trend\r\n");
    }

    // Formatiranje vremena (ili uptime-a ako sat ne radi)
    if (!rtc_ok) {
        append_text(row, &row_pos, "BOOT+");
        append_u32(row, &row_pos, uptime_seconds);
        append_text(row, &row_pos, "s");
    } else {
        append_text(row, &row_pos, "20");
        if (time->year < 10) append_char(row, &row_pos, '0');
        append_u32(row, &row_pos, time->year);
        append_char(row, &row_pos, '-');
        if (time->month < 10) append_char(row, &row_pos, '0');
        append_u32(row, &row_pos, time->month);
        append_char(row, &row_pos, '-');
        if (time->date < 10) append_char(row, &row_pos, '0');
        append_u32(row, &row_pos, time->date);
        append_char(row, &row_pos, ' ');
        if (time->hours < 10) append_char(row, &row_pos, '0');
        append_u32(row, &row_pos, time->hours);
        append_char(row, &row_pos, ':');
        if (time->minutes < 10) append_char(row, &row_pos, '0');
        append_u32(row, &row_pos, time->minutes);
        append_char(row, &row_pos, ':');
        if (time->seconds < 10) append_char(row, &row_pos, '0');
        append_u32(row, &row_pos, time->seconds);
    }

    // Dodavanje senzorskih podataka (Vlaga je izbačena jer je čip BMP280)
    append_char(row, &row_pos, ',');
    append_fixed_x100(row, &row_pos, bme->temperature_x100);
    append_char(row, &row_pos, ',');
    append_u32(row, &row_pos, bme->pressure_pa);
    append_char(row, &row_pos, ',');
    append_u32(row, &row_pos, lux);
    append_char(row, &row_pos, ',');
    append_text(row, &row_pos, trend == TREND_UP ? "UP" : (trend == TREND_DOWN ? "DOWN" : "STABLE"));
    append_text(row, &row_pos, "\r\n");

    // Isti CSV red ispisi i na Serial Monitor.
    uart_puts(row);

    // Ako novi red ne stane u trenutni sektor, zapiši ga na SD karticu
    if (buffer_pos + row_pos > 512) {
        while (buffer_pos < 512) sector_buffer[buffer_pos++] = 0x00; // Padding

        if (!sdcard_write_block(next_log_block, sector_buffer)) {
            return 0; // Greška pri zapisu
        }
        next_log_block++;
        buffer_pos = 0;
    }

    // Kopiraj podatke reda u sector buffer
    for (uint16_t i = 0; i < row_pos; i++) {
        sector_buffer[buffer_pos++] = (uint8_t)row[i];
    }

    return 1;
}

int main(void)
{
    uint8_t bme_ok, lux_ok, rtc_ok, sd_ok;
    
    // Inicijalizacija hardverskih pinova za SPI
    DDRB |= (1 << PB2) | (1 << PB3) | (1 << PB5);
    PORTB |= (1 << PB2); 

    uart_init();
    i2c_init();
    sei();

    _delay_ms(500); 

    uart_puts("--- Logger Pokrenut ---\r\n");

    // Inicijalizacija senzora
    bme_ok = bme280_init(0x76);
    if (!bme_ok) {
        bme_ok = bme280_init(0x77);
        if (bme_ok) bme_addr = 0x77;
    } else {
        bme_addr = 0x76;
    }
    
    lux_ok = bh1750_init(BH1750_ADDR);
    rtc_ok = ds3231_init(DS3231_ADDR);

    // Inicijalizacija SD kartice
    sd_ok = (sdcard_init_debug() == SD_ERR_NONE);

    // Minimalni i čisti statusni ispis na bootu
    uart_puts(bme_ok ? "BMP280: OK\r\n" : "BMP280: FAIL\r\n");
    uart_puts(lux_ok ? "BH1750: OK\r\n" : "BH1750: FAIL\r\n");
    uart_puts(rtc_ok ? "DS3231: OK\r\n" : "DS3231: FAIL\r\n");
    uart_puts(sd_ok  ? "SD Card: OK\r\n" : "SD Card: FAIL\r\n");

    while (1) {
        bme280_data_t bme_data = {0};
        uint16_t lux_data = 0;
        ds3231_time_t current_time = {0};
        pressure_trend_t trend = TREND_STABLE;

        // Čitanje senzora ako su ispravni
        if (bme_ok) {
            bme_ok = bme280_read_data(bme_addr, &bme_data);
            trend = calculate_pressure_trend(bme_data.pressure_pa);
        }
        if (lux_ok) {
            lux_ok = bh1750_read_lux(BH1750_ADDR, &lux_data);
        }
        if (rtc_ok) {
            rtc_ok = ds3231_read_time(DS3231_ADDR, &current_time);
        }

        // Zapisivanje na SD karticu
        if (sd_ok) {
            if (!log_record(&current_time, rtc_ok, &bme_data, lux_data, trend)) {
                uart_puts("[SD_ERR] Pokušaj re-inicijalizacije...\r\n");
                sd_ok = (sdcard_init() == 1);
            }
        }

        wait_log_interval();
    }
    return 0;
}
