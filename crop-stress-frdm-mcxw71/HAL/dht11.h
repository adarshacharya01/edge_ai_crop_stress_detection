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

void dht11_init(void);
bool dht11_read(float *temperature_c, float *humidity_pct);
int DHT11_Read(float *humidity_pct, float *temperature_c);
const char *dht11_get_status_str(void);
const char *dht11_get_active_pin_str(void);

#endif /* CROP_DHT11_H */
