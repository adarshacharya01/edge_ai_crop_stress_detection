#include "Sensors/sensors.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
#include "Config/config.h"
#include "HAL/hal.h"
#include "HAL/board_adc.h"

static bool s_bh1750_present = false;
static bool s_fxls8964_present = false;

static bool sht31_crc_ok(const uint8_t *bytes)
{
    uint8_t crc = 0xffU;
    for (unsigned i = 0; i < 2; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U)
                                : (uint8_t)(crc << 1);
        }
    }
    return crc == bytes[2];
}

static bool read_air(float *temperature_c, float *humidity_pct)
{
    const uint8_t command[] = {0x24U, 0x00U}; /* High repeatability measurement */
    uint8_t data[6];
    if (!hal_i2c_write(SHT31_I2C_ADDR, command, 2U)) {
        return false;
    }
    hal_delay_ms(20U);
    if (!hal_i2c_read(SHT31_I2C_ADDR, data, 6U) ||
        !sht31_crc_ok(data) || !sht31_crc_ok(data + 3)) {
        return false;
    }

    uint16_t raw_temp = ((uint16_t)data[0] << 8) | data[1];
    uint16_t raw_rh   = ((uint16_t)data[3] << 8) | data[4];
    *temperature_c = -45.0f + 175.0f * (float)raw_temp / 65535.0f;
    *humidity_pct  = 100.0f * (float)raw_rh / 65535.0f;
    return true;
}

static bool read_light(float *lux)
{
    /* If external digital BH1750 is detected, read from it */
    if (s_bh1750_present) {
        uint8_t data[2];
        if (hal_i2c_read(BH1750_I2C_ADDR, data, 2U)) {
            *lux = (((uint16_t)data[0] << 8) | data[1]) / 1.2f;
            return true;
        }
    }

    /* Fallback: Read from FRDM-MCXW71 onboard ALS-PT19 phototransistor (ADC0_B2) */
    float mv = 0.0f;
    if (crop_adc_read_light_sensor_mv(&mv)) {
        /*
         * ALS-PT19 phototransistor output voltage across load resistor:
         * In ambient daylight, voltage typically spans 0 to 3000 mV.
         * Approximate conversion: ~10 lux per mV for general agronomic thresholds.
         */
        *lux = fmaxf(0.0f, mv * 12.5f);
        return true;
    }

    return false;
}

static bool read_soil(float *soil_pct)
{
    float mv;
    if (!hal_soil_millivolts(&mv) || !isfinite(mv) ||
        (CROP_SOIL_WET_MV == CROP_SOIL_DRY_MV)) {
        return false;
    }
    float pct = 100.0f * (mv - CROP_SOIL_DRY_MV) /
                (CROP_SOIL_WET_MV - CROP_SOIL_DRY_MV);
    *soil_pct = fmaxf(0.0f, fminf(100.0f, pct));
    return true;
}

static bool read_accel(float *x_g, float *y_g, float *z_g)
{
    if (!s_fxls8964_present) {
        *x_g = 0.0f; *y_g = 0.0f; *z_g = 1.0f;
        return true;
    }

    /* OUT_X_LSB (reg 0x04) to OUT_Z_MSB (reg 0x09) */
    uint8_t reg = 0x04U;
    uint8_t buf[6];
    if (hal_i2c_write(FXLS8964_I2C_ADDR, &reg, 1U) &&
        hal_i2c_read(FXLS8964_I2C_ADDR, buf, 6U)) {
        int16_t raw_x = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        int16_t raw_y = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
        int16_t raw_z = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);

        /* FXLS8964 12-bit output in +/- 2g mode: 0.98 mg/LSB (~1024 LSB/g) */
        *x_g = (float)raw_x / 1024.0f;
        *y_g = (float)raw_y / 1024.0f;
        *z_g = (float)raw_z / 1024.0f;
        return true;
    }
    return false;
}

bool sensors_init(void)
{
    /* 1. Check & configure external BH1750 continuous high-resolution mode (0x10) */
    const uint8_t continuous_high_resolution = 0x10U;
    if (hal_i2c_write(BH1750_I2C_ADDR, &continuous_high_resolution, 1U)) {
        s_bh1750_present = true;
        hal_delay_ms(180U);
    } else {
        s_bh1750_present = false; /* Will use onboard ALS-PT19 instead */
    }

    /* 2. Check & configure onboard FXLS8964AF Accelerometer (WHO_AM_I reg 0x13) */
    uint8_t whoami_reg = 0x13U;
    uint8_t whoami_val = 0U;
    if (hal_i2c_write(FXLS8964_I2C_ADDR, &whoami_reg, 1U) &&
        hal_i2c_read(FXLS8964_I2C_ADDR, &whoami_val, 1U)) {
        /* Set SENS_CONFIG1 (reg 0x15) to ACTIVE mode */
        uint8_t cfg_cmd[2] = {0x15U, 0x01U};
        hal_i2c_write(FXLS8964_I2C_ADDR, cfg_cmd, 2U);
        s_fxls8964_present = true;
    } else {
        s_fxls8964_present = false;
    }

    return true;
}

bool sensors_read(crop_reading_t *reading)
{
    if (reading == NULL) {
        return false;
    }
    memset(reading, 0, sizeof(crop_reading_t));

    bool soil_ok  = read_soil(&reading->soil_pct);
    bool air_ok   = read_air(&reading->temperature_c, &reading->humidity_pct);
    bool light_ok = read_light(&reading->light_lux);
    read_accel(&reading->accel_x_g, &reading->accel_y_g, &reading->accel_z_g);

    return soil_ok && air_ok && light_ok;
}
