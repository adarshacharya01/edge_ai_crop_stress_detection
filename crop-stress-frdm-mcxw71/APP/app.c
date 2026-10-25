#include "APP/app.h"

#include <stdio.h>
#include "Config/config.h"
#include "HAL/hal.h"
#include "Model/model.h"
#include "Sensors/sensors.h"

int app_run(void)
{
    printf("========================================\r\n");
    printf(" FRDM-MCXW71 Crop Stress Monitor (Edge AI)\r\n");
    printf("========================================\r\n");

    if (!hal_init() || !sensors_init()) {
        printf("Startup failed: check board configuration, LPADC and I2C bus.\r\n");
        return 1;
    }

    printf("Initialization successful. Starting measurement loop...\r\n");

#if defined(CROP_SIMULATOR)
    for (unsigned sample = 0; sample < 3; ++sample) {
#else
    for (;;) {
#endif
        crop_reading_t reading;
        if (sensors_read(&reading)) {
            float p = model_stress_probability(&reading);
            printf("Soil: %5.1f%% | Temp: %4.1f C | RH: %4.1f%% | Light: %6.0f lx | Stress: %4.2f | %s\r\n",
                   reading.soil_pct, reading.temperature_c,
                   reading.humidity_pct, reading.light_lux, p,
                   (p >= CROP_ALERT_PROBABILITY) ? ">>> ALERT: CHECK PLANT <<<" : "Normal");

            /* If onboard accelerometer reading is active, print orientation info */
            if (reading.accel_x_g != 0.0f || reading.accel_y_g != 0.0f || reading.accel_z_g != 1.0f) {
                printf("  [Onboard FXLS8964] X: %+.2fg | Y: %+.2fg | Z: %+.2fg\r\n",
                       reading.accel_x_g, reading.accel_y_g, reading.accel_z_g);
            }
        } else {
            printf("Sensor read failed: check power, wiring, and I2C addresses\r\n");
        }
        hal_delay_ms(CROP_SAMPLE_INTERVAL_MS);
    }
    return 0;
}
