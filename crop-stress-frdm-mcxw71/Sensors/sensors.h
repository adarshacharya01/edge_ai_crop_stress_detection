#ifndef CROP_SENSORS_H
#define CROP_SENSORS_H

#include <stdbool.h>

typedef struct {
    float soil_pct;       /* Soil moisture percentage: 0.0% to 100.0% */
    float soil_raw_mv;    /* Soil sensor raw millivolts measured at A0 */
    bool  soil_valid;     /* True if soil sensor read was successful */

    float temperature_c;  /* Ambient air temperature in degrees Celsius */
    float humidity_pct;   /* Relative humidity: 0.0% to 100.0% */
    bool  air_valid;      /* True if DHT11 sensor read was successful */

    float light_lux;      /* Ambient illuminance in Lux */
    bool  light_valid;    /* True if light sensor read was successful */

    float accel_x_g;      /* Onboard FXLS8964AF X-axis acceleration (g) */
    float accel_y_g;      /* Onboard FXLS8964AF Y-axis acceleration (g) */
    float accel_z_g;      /* Onboard FXLS8964AF Z-axis acceleration (g) */
    bool  accel_valid;    /* True if accelerometer read was successful */
} crop_reading_t;

#endif /* CROP_SENSORS_H */
