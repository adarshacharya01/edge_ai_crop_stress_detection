#ifndef CROP_BOARD_ADC_H
#define CROP_BOARD_ADC_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes VREF and LPADC (ADC0) with auto-calibration on FRDM-MCXW71.
 * Configured identically to the NXP App Code Hub example.
 */
bool crop_adc_init(void);

/**
 * @brief Reads voltage from the external capacitive soil moisture probe.
 * Defaults to ADC0 Channel 5 Side B (Arduino Header A0 / Pin PTD1).
 *
 * @param[out] millivolts Pointer to store measured voltage in millivolts.
 * @return true if conversion succeeded, false on error.
 */
bool crop_adc_read_mv(float *millivolts);

/**
 * @brief Reads voltage from the FRDM-MCXW71 onboard ALS-PT19 light sensor.
 * Connected to ADC0 Channel 2 Side B.
 *
 * @param[out] millivolts Pointer to store measured phototransistor voltage in millivolts.
 * @return true if conversion succeeded, false on error.
 */
bool crop_adc_read_light_sensor_mv(float *millivolts);

/**
 * @brief Generic LPADC channel conversion on MCXW71.
 *
 * @param channel LPADC channel number (0-15).
 * @param is_side_b True for Side B, false for Side A.
 * @param[out] millivolts Output millivolts.
 * @return true if conversion succeeded, false on error.
 */
bool crop_adc_read_channel_mv(uint32_t channel, bool is_side_b, float *millivolts);

#endif /* CROP_BOARD_ADC_H */
