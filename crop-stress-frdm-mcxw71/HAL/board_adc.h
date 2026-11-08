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

typedef struct {
    float a0_mv;
    float a1_mv;
    float a2_mv;
    float a3_mv;
    float a4_mv;
    float a5_mv;
    int best_pin_idx;
} crop_adc_all_pins_t;

/**
 * @brief Scan all 6 Arduino analog pins (A0 to A5) on Header J4.
 */
bool crop_adc_scan_all(crop_adc_all_pins_t *out);

/**
 * @brief Return the last scanned voltages for display.
 */
const crop_adc_all_pins_t *crop_adc_get_last_scan(void);

#endif /* CROP_BOARD_ADC_H */
