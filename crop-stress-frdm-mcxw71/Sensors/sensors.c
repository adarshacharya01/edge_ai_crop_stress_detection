#if __has_include("Sensors/sensors.h")
#include "Sensors/sensors.h"
#include "Config/config.h"
#include "HAL/hal.h"
#include "HAL/board_adc.h"
#else
#include "sensors.h"
#include "config.h"
#include "hal.h"
#include "board_adc.h"
#endif

#include <math.h>
#include <stdint.h>
#include <string.h>

static bool s_bh1750_present = false;
static bool s_fxls8964_present = false;

static bool read_air(float *temperature_c, float *humidity_pct)
{
    /* Read temperature and humidity from DHT11 on J1 Pin 3 (D2 / PTC0) */
    if (hal_dht11_read(temperature_c, humidity_pct)) {
        return true;
    }
    /* Fallback default values if reading times out or disconnected */
    *temperature_c = 25.0f;
    *humidity_pct  = 50.0f;
    return false;
}

static bool read_light(float *lux)
{
    /* 1. If external digital BH1750 is detected, read from it */
    if (s_bh1750_present) {
        uint8_t data[2];
        if (hal_i2c_read(BH1750_I2C_ADDR, data, 2U)) {
            *lux = (((uint16_t)data[0] << 8) | data[1]) / 1.2f;
            return true;
        }
    }

    /* 2. Fallback: Read from FRDM-MCXW71 onboard ALS-PT19 phototransistor (ADC0_B2) */
    float mv = 0.0f;
    if (crop_adc_read_light_sensor_mv(&mv)) {
        *lux = fmaxf(0.0f, mv * 12.5f);
        return true;
    }

    /* 3. Default daytime illuminance if not readable */
    *lux = 500.0f;
    return false;
}

static bool read_soil(float *soil_pct, float *raw_mv)
{
    float mv = 0.0f;
    if (!hal_soil_millivolts(&mv) || !isfinite(mv)) {
        *soil_pct = 50.0f;
        if (raw_mv != NULL) {
            *raw_mv = 0.0f;
        }
        return false;
    }

    if (raw_mv != NULL) {
        *raw_mv = mv;
    }

    if (CROP_SOIL_WET_MV == CROP_SOIL_DRY_MV) {
        *soil_pct = 50.0f;
        return true;
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
        return false;
    }

    uint8_t reg = 0x04U;
    uint8_t buf[6];
    if (hal_i2c_write(FXLS8964_I2C_ADDR, &reg, 1U) &&
        hal_i2c_read(FXLS8964_I2C_ADDR, buf, 6U)) {
        int16_t raw_x = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        int16_t raw_y = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
        int16_t raw_z = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);

        *x_g = (float)raw_x / 1024.0f;
        *y_g = (float)raw_y / 1024.0f;
        *z_g = (float)raw_z / 1024.0f;
        return true;
    }
    return false;
}

bool sensors_init(void)
{
    /* External BH1750 check */
    const uint8_t continuous_high_resolution = 0x10U;
    if (hal_i2c_write(BH1750_I2C_ADDR, &continuous_high_resolution, 1U)) {
        s_bh1750_present = true;
        hal_delay_ms(180U);
    } else {
        s_bh1750_present = false;
    }

    /* Onboard FXLS8964AF Accelerometer check */
    uint8_t whoami_reg = 0x13U;
    uint8_t whoami_val = 0U;
    if (hal_i2c_write(FXLS8964_I2C_ADDR, &whoami_reg, 1U) &&
        hal_i2c_read(FXLS8964_I2C_ADDR, &whoami_val, 1U)) {
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

    reading->soil_valid  = read_soil(&reading->soil_pct, &reading->soil_raw_mv);
    reading->air_valid   = read_air(&reading->temperature_c, &reading->humidity_pct);
    reading->light_valid = read_light(&reading->light_lux);
    reading->accel_valid = read_accel(&reading->accel_x_g, &reading->accel_y_g, &reading->accel_z_g);

    return true;
}
