#include "HAL/hal.h"

#include <math.h>
#include <stddef.h>

static unsigned sample_index;
static bool light_started;
static bool air_triggered;

static uint8_t sht_crc(const uint8_t *bytes)
{
    uint8_t crc = 0xffU;
    for (unsigned i = 0; i < 2; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U)
                                  : (uint8_t)(crc << 1);
    }
    return crc;
}

bool hal_init(void)
{
    sample_index = 0;
    light_started = false;
    air_triggered = false;
    return true;
}

void hal_delay_ms(uint32_t milliseconds)
{
    if (milliseconds == 5000U && sample_index < 2U)
        ++sample_index;
}

bool hal_i2c_write(uint8_t address, const uint8_t *data, uint32_t size)
{
    if (data == NULL)
        return false;
    if (address == 0x23U && size == 1U && data[0] == 0x10U) {
        light_started = true;
        return true;
    }
    if (address == 0x44U && size == 2U &&
        data[0] == 0x24U && data[1] == 0x00U) {
        air_triggered = true;
        return true;
    }
    return false;
}

bool hal_i2c_read(uint8_t address, uint8_t *data, uint32_t size)
{
    static const float temperature[] = {25.0f, 30.0f, 34.0f};
    static const float humidity[] = {65.0f, 49.0f, 35.0f};
    static const float illuminance[] = {400.0f, 820.0f, 1200.0f};
    if (data == NULL)
        return false;
    if (address == 0x44U && size == 6U && air_triggered) {
        air_triggered = false;
        uint16_t t = (uint16_t)lroundf((temperature[sample_index] + 45.0f) *
                                       65535.0f / 175.0f);
        uint16_t h = (uint16_t)lroundf(humidity[sample_index] * 65535.0f / 100.0f);
        data[0] = (uint8_t)(t >> 8);
        data[1] = (uint8_t)t;
        data[2] = sht_crc(data);
        data[3] = (uint8_t)(h >> 8);
        data[4] = (uint8_t)h;
        data[5] = sht_crc(data + 3);
        return true;
    }
    if (address == 0x23U && size == 2U && light_started) {
        uint16_t raw = (uint16_t)lroundf(illuminance[sample_index] * 1.2f);
        data[0] = (uint8_t)(raw >> 8);
        data[1] = (uint8_t)raw;
        return true;
    }
    return false;
}

bool hal_soil_millivolts(float *millivolts)
{
    static const float samples[] = {2200.0f, 1700.0f, 1300.0f};
    if (millivolts == NULL)
        return false;
    *millivolts = samples[sample_index];
    return true;
}
