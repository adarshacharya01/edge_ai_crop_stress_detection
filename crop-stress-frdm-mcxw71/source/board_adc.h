#ifndef CROP_BOARD_ADC_H
#define CROP_BOARD_ADC_H

#include <stdbool.h>
#include <stdint.h>

bool crop_adc_init(void);
bool crop_adc_read_channel_mv(uint32_t channel, bool is_side_b, float *millivolts);

uint32_t BOARD_ReadSoilAdcMv(void);
float CalculateSoilMoisturePercent(uint32_t raw_adc_mv);
uint32_t BOARD_ReadAmbientLightLux(void);
float CalculateCropStressScore(float live_soil, float live_temp, float live_hum, float live_lux);

bool crop_adc_read_mv(float *millivolts);
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

bool crop_adc_scan_all(crop_adc_all_pins_t *out);
const crop_adc_all_pins_t *crop_adc_get_last_scan(void);

#endif /* CROP_BOARD_ADC_H */
