#ifndef CROP_CONFIG_H
#define CROP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* Application Sampling & Alert Thresholds */
#define CROP_SAMPLE_INTERVAL_MS    5000U
#define CROP_ALERT_PROBABILITY     0.70f

/* Soil Moisture Sensor Voltage Calibration (Millivolts) */
#ifndef CROP_SOIL_DRY_MV
#define CROP_SOIL_DRY_MV           2600.0f  /* Measured dry-air probe voltage (~2.6V) */
#endif

#ifndef CROP_SOIL_WET_MV
#define CROP_SOIL_WET_MV           1100.0f  /* Measured in water probe voltage (~1.1V) */
#endif

/* --------------------------------------------------------------------------
 * FRDM-MCXW71 Hardware Settings (Derived from NXP App Code Hub Demo)
 * Repository: nxp-appcodehub/dm-mcxw71-accel-and-light-ble
 * -------------------------------------------------------------------------- */

/* LPI2C Configuration
 * In FRDM-MCXW71:
 * - LPI2C1 is routed to PTB4 (SDA) and PTB5 (SCL)
 * - Clock source is FRO192M divided by 16 = 12 MHz
 * - Connects to onboard FXLS8964AF accelerometer and Arduino header I2C pins
 */
#ifndef CROP_I2C_BASE
#define CROP_I2C_BASE              LPI2C1
#endif

#ifndef CROP_I2C_SOURCE_CLOCK_HZ
#define CROP_I2C_SOURCE_CLOCK_HZ   12000000U /* 192 MHz FRO / 16 */
#endif

#ifndef CROP_I2C_BAUDRATE_HZ
#define CROP_I2C_BAUDRATE_HZ       100000U   /* 100 kHz Standard Mode */
#endif

/* LPADC Configuration
 * In FRDM-MCXW71:
 * - ADC0 is the on-chip LPADC peripheral
 * - Reference Voltage: 3300 mV (VDD reference)
 * - Soil Sensor: Arduino Header A0 (PTD1 -> ADC0_B5 / Channel 5 Side B)
 * - Onboard Light Sensor: ALS-PT19 (ADC0_B2 / Channel 2 Side B)
 */
#ifndef CROP_ADC_BASE
#define CROP_ADC_BASE              ADC0
#endif

#ifndef CROP_ADC_VREF_MV
#define CROP_ADC_VREF_MV           3300.0f   /* 3.3V reference voltage */
#endif

/* Soil probe: Pin PTD1 = Arduino A0 = ADC0 Channel 5, Side B */
#define CROP_ADC_SOIL_CHANNEL      5U
#define CROP_ADC_SOIL_SIDE_B       1U

/* Onboard phototransistor light sensor: ALS-PT19 = ADC0 Channel 2, Side B */
#define CROP_ADC_LIGHT_CHANNEL     2U
#define CROP_ADC_LIGHT_SIDE_B      1U

/* I2C Sensor Addresses (7-bit) */
#define SHT31_I2C_ADDR             0x44U     /* SHT31 Temp & Humidity sensor */
#define BH1750_I2C_ADDR            0x23U     /* BH1750 Ambient light sensor */
#define FXLS8964_I2C_ADDR          0x19U     /* Onboard FXLS8964AF accelerometer */

#endif /* CROP_CONFIG_H */
