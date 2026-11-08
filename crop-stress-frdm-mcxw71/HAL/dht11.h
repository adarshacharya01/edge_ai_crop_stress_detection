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

typedef enum {
    DHT11_STATUS_OK = 0,
    DHT11_STATUS_IDLE_LOW,
    DHT11_STATUS_NO_RESPONSE_LOW,
    DHT11_STATUS_NO_RESPONSE_HIGH,
    DHT11_STATUS_BIT_TIMEOUT,
    DHT11_STATUS_CHECKSUM_ERROR
} dht11_status_t;

/**
 * @brief Initialize the GPIO pins (D2/D3/D4) with internal pull-up and enable DWT counter.
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

/**
 * @brief Return a human-readable diagnostic message describing the last read attempt.
 */
const char *dht11_get_status_str(void);

/**
 * @brief Return which pin last succeeded or was attempted ("D2 (PTC4)", "D4 (PTA19)", etc.).
 */
const char *dht11_get_active_pin_str(void);

#endif /* CROP_DHT11_H */
