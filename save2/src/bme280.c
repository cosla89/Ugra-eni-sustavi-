#include "../drivers/bme280.h"

#include "i2c.h"

typedef struct {
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;
    uint8_t dig_h1;
    int16_t dig_h2;
    uint8_t dig_h3;
    int16_t dig_h4;
    int16_t dig_h5;
    int8_t dig_h6;
    int32_t t_fine;
} bme280_calib_t;

static bme280_calib_t calib;
static uint8_t has_humidity = 0;

static uint8_t bme280_write_reg(uint8_t address, uint8_t reg, uint8_t value)
{
    if (!i2c_start((address << 1) | 0)) {
        i2c_stop();
        return 0;
    }
    if (!i2c_write(reg) || !i2c_write(value)) {
        i2c_stop();
        return 0;
    }
    i2c_stop();
    return 1;
}

static uint8_t bme280_read_regs(uint8_t address, uint8_t reg, uint8_t *data, uint8_t len)
{
    if (!i2c_start((address << 1) | 0)) {
        i2c_stop();
        return 0;
    }
    if (!i2c_write(reg)) {
        i2c_stop();
        return 0;
    }
    if (!i2c_start((address << 1) | 1)) {
        i2c_stop();
        return 0;
    }
    for (uint8_t i = 0; i < len; i++) {
        data[i] = (i == (len - 1)) ? i2c_read_nack() : i2c_read_ack();
    }
    i2c_stop();
    return 1;
}

static uint16_t u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t s16_le(const uint8_t *data)
{
    return (int16_t)u16_le(data);
}

static int16_t sign_extend_12(uint16_t value)
{
    return (value & 0x0800) ? (int16_t)(value | 0xF000) : (int16_t)value;
}

static uint8_t bme280_read_calibration(uint8_t address)
{
    uint8_t c1[26];
    uint8_t c2[7];

    if (!bme280_read_regs(address, 0x88, c1, sizeof(c1))) {
        return 0;
    }

    calib.dig_t1 = u16_le(&c1[0]);
    calib.dig_t2 = s16_le(&c1[2]);
    calib.dig_t3 = s16_le(&c1[4]);
    calib.dig_p1 = u16_le(&c1[6]);
    calib.dig_p2 = s16_le(&c1[8]);
    calib.dig_p3 = s16_le(&c1[10]);
    calib.dig_p4 = s16_le(&c1[12]);
    calib.dig_p5 = s16_le(&c1[14]);
    calib.dig_p6 = s16_le(&c1[16]);
    calib.dig_p7 = s16_le(&c1[18]);
    calib.dig_p8 = s16_le(&c1[20]);
    calib.dig_p9 = s16_le(&c1[22]);

    calib.dig_h1 = 0;
    calib.dig_h2 = 0;
    calib.dig_h3 = 0;
    calib.dig_h4 = 0;
    calib.dig_h5 = 0;
    calib.dig_h6 = 0;

    if (has_humidity) {
        if (!bme280_read_regs(address, 0xE1, c2, sizeof(c2))) {
            return 0;
        }
        calib.dig_h1 = c1[25];
        calib.dig_h2 = s16_le(&c2[0]);
        calib.dig_h3 = c2[2];
        calib.dig_h4 = sign_extend_12(((uint16_t)c2[3] << 4) | (c2[4] & 0x0F));
        calib.dig_h5 = sign_extend_12(((uint16_t)c2[5] << 4) | (c2[4] >> 4));
        calib.dig_h6 = (int8_t)c2[6];
    }

    return 1;
}

static int32_t compensate_temperature(int32_t adc_t)
{
    int32_t var1 = ((((adc_t >> 3) - ((int32_t)calib.dig_t1 << 1))) *
                    ((int32_t)calib.dig_t2)) >> 11;
    int32_t var2 = (((((adc_t >> 4) - ((int32_t)calib.dig_t1)) *
                      ((adc_t >> 4) - ((int32_t)calib.dig_t1))) >> 12) *
                    ((int32_t)calib.dig_t3)) >> 14;
    calib.t_fine = var1 + var2;
    return (calib.t_fine * 5 + 128) >> 8;
}

static uint32_t compensate_pressure(int32_t adc_p)
{
    int64_t var1 = ((int64_t)calib.t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)calib.dig_p6;
    var2 = var2 + ((var1 * (int64_t)calib.dig_p5) << 17);
    var2 = var2 + (((int64_t)calib.dig_p4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib.dig_p3) >> 8) +
           ((var1 * (int64_t)calib.dig_p2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_p1) >> 33;

    if (var1 == 0) {
        return 0;
    }

    int64_t p = 1048576 - adc_p;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib.dig_p9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib.dig_p8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib.dig_p7) << 4);
    return (uint32_t)(p >> 8);
}

static uint32_t compensate_humidity(int32_t adc_h)
{
    int32_t v_x1 = calib.t_fine - 76800;
    v_x1 = (((((adc_h << 14) - (((int32_t)calib.dig_h4) << 20) -
               (((int32_t)calib.dig_h5) * v_x1)) + 16384) >> 15) *
            (((((((v_x1 * ((int32_t)calib.dig_h6)) >> 10) *
                 (((v_x1 * ((int32_t)calib.dig_h3)) >> 11) + 32768)) >> 10) +
               2097152) * ((int32_t)calib.dig_h2) + 8192) >> 14));
    v_x1 = v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) *
                    ((int32_t)calib.dig_h1)) >> 4);
    if (v_x1 < 0) {
        v_x1 = 0;
    }
    if (v_x1 > 419430400) {
        v_x1 = 419430400;
    }
    return (uint32_t)(v_x1 >> 12) * 100UL / 1024UL;
}

uint8_t bme280_init(uint8_t address)
{
    uint8_t id = 0;
    if (!bme280_read_id(address, &id)) {
        return 0;
    }

    if (id == 0x60) {
        has_humidity = 1;
    } else if (id == 0x58) {
        has_humidity = 0;
    } else {
        return 0;
    }

    if (!bme280_read_calibration(address)) {
        return 0;
    }

    if (has_humidity && !bme280_write_reg(address, 0xF2, 0x01)) {
        return 0;
    }
    if (!bme280_write_reg(address, 0xF5, 0xA0)) {
        return 0;
    }
    return bme280_write_reg(address, 0xF4, 0x27);
}

uint8_t bme280_read_id(uint8_t address, uint8_t *id)
{
    return bme280_read_regs(address, 0xD0, id, 1);
}

uint8_t bme280_read_data(uint8_t address, bme280_data_t *data)
{
    uint8_t raw[8];
    uint8_t len = has_humidity ? 8 : 6;

    if (!bme280_read_regs(address, 0xF7, raw, len)) {
        return 0;
    }

    int32_t adc_p = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_t = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);

    data->temperature_x100 = compensate_temperature(adc_t);
    data->pressure_pa = compensate_pressure(adc_p);

    if (has_humidity) {
        int32_t adc_h = ((int32_t)raw[6] << 8) | raw[7];
        data->humidity_x100 = compensate_humidity(adc_h);
    } else {
        data->humidity_x100 = 0;
    }

    return 1;
}
