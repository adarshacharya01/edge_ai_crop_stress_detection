/**
 * @file dht11.h
 * @brief High-precision DHT11 sensor driver for FRDM-MCXW71 using DWT cycle counter.
 *
 * Hardware connection:
 *   DHT11 DATA  ---> J1 Pin 3 (D2 / PTC0)
 *   DHT11 VCC   ---> J3 Pin 4 (3.3V)
 *   DHT11 GND   ---> J3 Pin 6 or Pin 7 (GND)
 */
#ifndef CROP_DHT11_H
#define CROP_DHT11_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the PTC0 GPIO pin with internal pull-up and enable DWT counter.
 */
void dht11_init(void);

/**
 * @brief Read temperature and humidity from DHT11.
 *
 * @param[out] temperature_c Pointer to store temperature in °C.
 * @param[out] humidity_pct  Pointer to store relative humidity in %.
 * @return true on success (checksum verified), false on timeout or checksum mismatch.
 */
bool dht11_read(float *temperature_c, float *humidity_pct);

#endif /* CROP_DHT11_H */
