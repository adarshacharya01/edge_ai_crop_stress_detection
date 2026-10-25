#ifndef CROP_SENSORS_H
#define CROP_SENSORS_H

#include <stdbool.h>

typedef struct {
    float soil_pct;       /* Soil moisture percentage: 0.0% to 100.0% */
    float temperature_c;  /* Ambient air temperature in degrees Celsius */
    float humidity_pct;   /* Relative humidity: 0.0% to 100.0% */
    float light_lux;      /* Ambient illuminance in Lux */
    float accel_x_g;      /* Onboard FXLS8964AF X-axis acceleration (g) */
    float accel_y_g;      /* Onboard FXLS8964AF Y-axis acceleration (g) */
    float accel_z_g;      /* Onboard FXLS8964AF Z-axis acceleration (g) */
} crop_reading_t;

bool sensors_init(void);
bool sensors_read(crop_reading_t *reading);

#endif /* CROP_SENSORS_H */
