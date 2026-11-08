/**
 * @file dht11.h
 * @brief High-precision DHT11 single-wire temperature & humidity driver for FRDM-MCXW71.
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
 * @brief Initialize D2 (PTC0) as Open-Drain bidirectional GPIO with internal pull-up and DWT timer.
 */
void dht11_init(void);

/**
 * @brief Read temperature (°C) and humidity (%RH) from DHT11.
 *
 * @param[out] temperature_c Pointer to store temperature in °C.
 * @param[out] humidity_pct  Pointer to store relative humidity in %.
 * @return true on success (checksum verified), false on error or timeout.
 */
bool dht11_read(float *temperature_c, float *humidity_pct);

/**
 * @brief Get human-readable diagnostic status string of the last read attempt.
 */
const char *dht11_get_status_str(void);

/**
 * @brief Get the name of the active pin used.
 */
const char *dht11_get_active_pin_str(void);

#endif /* CROP_DHT11_H */
