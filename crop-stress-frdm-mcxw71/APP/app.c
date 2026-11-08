#if __has_include("APP/app.h")
#include "APP/app.h"
#include "Config/config.h"
#include "HAL/hal.h"
#include "Model/model.h"
#include "Sensors/sensors.h"
#else
#include "app.h"
#include "config.h"
#include "hal.h"
#include "model.h"
#include "sensors.h"
#endif

#include "fsl_debug_console.h"
#include "board.h"
#include <stdio.h>
#include <math.h>

/* Helper to safely print floating point numbers using integer arithmetic
 * (Guarantees output even on minimal C libraries without float printf support) */
static void print_float(float val, int decimals)
{
    if (isnan(val)) {
        PRINTF("NaN");
        return;
    }
    if (val < 0.0f) {
        PRINTF("-");
        val = -val;
    }
    int int_part = (int)val;
    PRINTF("%d.", int_part);

    float remainder = val - (float)int_part;
    for (int i = 0; i < decimals; ++i) {
        remainder *= 10.0f;
        int d = (int)remainder;
        PRINTF("%d", d);
        remainder -= (float)d;
    }
}

int app_run(void)
{
    /* 1. Hardware initialization MUST happen before any UART output */
    bool hal_ok = hal_init();

    /* 2. Now debug console UART is active (LPUART1 @ 115200 baud) */
    PRINTF("\r\n\r\n");
    PRINTF("================================================================\r\n");
    PRINTF("      🌱  FRDM-MCXW71 EDGE AI CROP STRESS MONITOR  🌱          \r\n");
    PRINTF("================================================================\r\n");
    PRINTF(" [INFO] MCU           : NXP MCXW716C (Cortex-M33 @ 96 MHz)\r\n");
    PRINTF(" [INFO] Serial Speed  : 115200 baud, 8-N-1\r\n");
    PRINTF(" [INFO] DHT11 Pin     : J1 Pin 3 (D2 / PTC0)\r\n");
    PRINTF(" [INFO] HW-103 Pin    : J4 Pin 1 (A0 / PTD1 -> LPADC0 Ch5 Side B)\r\n");
    PRINTF(" [INFO] Light Sensor  : Onboard ALS-PT19 Phototransistor\r\n");
    PRINTF("----------------------------------------------------------------\r\n");

    if (!hal_ok) {
        PRINTF(" [ERROR] HAL initialization failed! Check LPADC / VREF clock.\r\n");
    } else {
        PRINTF(" [OK] Peripheral initialization complete.\r\n");
    }

    sensors_init();
    PRINTF(" [OK] Sensor subsystems initialized.\r\n");
    PRINTF(" [INFO] Starting continuous sampling loop...\r\n");
    PRINTF("================================================================\r\n\r\n");

    uint32_t sample_count = 0U;

    for (;;) {
        ++sample_count;
        crop_reading_t reading;
        sensors_read(&reading);

        /* Run Edge AI lightweight Logistic Regression model */
        float stress_p = model_stress_probability(&reading);
        bool is_stressed = (stress_p >= CROP_ALERT_PROBABILITY);

        /* -------------------------------------------------------------
         * Live Serial Dashboard
         * ----------------------------------------------------------- */
        PRINTF("------------------------ [ SAMPLE #%u ] ------------------------\r\n", (unsigned int)sample_count);

        /* 1. Temperature & Humidity (DHT11) */
        PRINTF(" 🌡️  DHT11 (J1 Pin 3 / D2) :\r\n");
        if (reading.air_valid) {
            float temp_f = (reading.temperature_c * 9.0f / 5.0f) + 32.0f;
            PRINTF("     Temperature : ");
            print_float(reading.temperature_c, 1);
            PRINTF(" *C  (");
            print_float(temp_f, 1);
            PRINTF(" *F)\r\n");

            PRINTF("     Humidity    : ");
            print_float(reading.humidity_pct, 1);
            PRINTF(" %%RH\r\n");
        } else {
            PRINTF("     [!] Waiting for data / check D2 wire & 3.3V connection\r\n");
        }

        /* 2. Soil Moisture (HW-103) */
        PRINTF(" 💧 Soil Moisture (J4 Pin 1 / A0) :\r\n");
        if (reading.soil_valid) {
            PRINTF("     Moisture    : ");
            print_float(reading.soil_pct, 1);
            PRINTF(" %%  [Raw: ");
            print_float(reading.soil_raw_mv, 0);
            PRINTF(" mV]\r\n");

            const char *soil_desc = "Optimal";
            if (reading.soil_pct < 25.0f) {
                soil_desc = "DRY (Needs water!)";
            } else if (reading.soil_pct > 80.0f) {
                soil_desc = "VERY WET / Saturated";
            } else {
                soil_desc = "NORMAL / MOIST";
            }
            PRINTF("     Condition   : %s\r\n", soil_desc);
        } else {
            PRINTF("     [!] ADC conversion error (Check A0 probe connection)\r\n");
        }

        /* 3. Ambient Light */
        PRINTF(" ☀️  Ambient Light (Onboard ALS-PT19) :\r\n");
        PRINTF("     Illuminance : ");
        print_float(reading.light_lux, 0);
        PRINTF(" Lux\r\n");

        /* 4. Edge AI Plant Stress Inference */
        PRINTF(" 🤖 Edge AI Crop Health Analysis :\r\n");
        PRINTF("     Stress Score: ");
        print_float(stress_p * 100.0f, 1);
        PRINTF(" %%\r\n");

        if (is_stressed) {
            PRINTF("     STATUS      : >>> ⚠️ ALERT: CROP STRESS DETECTED! <<<\r\n");
            PRINTF("     ACTION      : ");
            if (reading.soil_pct < 30.0f) {
                PRINTF("Irrigate immediately — soil is too dry!\r\n");
            } else if (reading.temperature_c > 35.0f) {
                PRINTF("High temperature heat stress! Provide shade/ventilation.\r\n");
            } else {
                PRINTF("Inspect soil moisture, temperature, and sun exposure.\r\n");
            }
        } else {
            PRINTF("     STATUS      : [ HEALTHY - Optimal Growth Conditions ]\r\n");
        }

        PRINTF("----------------------------------------------------------------\r\n\r\n");

        /* Wait until next sample interval */
        hal_delay_ms(CROP_SAMPLE_INTERVAL_MS);
    }

    return 0;
}
