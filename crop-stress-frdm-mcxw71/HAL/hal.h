#ifndef CROP_HAL_H
#define CROP_HAL_H

#include <stdbool.h>
#include <stdint.h>

bool hal_init(void);
void hal_delay_ms(uint32_t milliseconds);
bool hal_i2c_write(uint8_t address, const uint8_t *data, uint32_t size);
bool hal_i2c_read(uint8_t address, uint8_t *data, uint32_t size);
bool hal_soil_millivolts(float *millivolts);

/* DHT11 temperature & humidity sensor (J1 Pin 3 / D2 / PTC0) */
bool hal_dht11_read(float *temperature_c, float *humidity_pct);

#endif
