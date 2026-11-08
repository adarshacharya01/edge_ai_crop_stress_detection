#if __has_include("APP/app.h")
#include "APP/app.h"
#include "APP/crop_stress.h"
#include "Config/config.h"
#include "HAL/hal.h"
#include "HAL/dht11.h"
#include "HAL/board_adc.h"
#include "Model/model.h"
#include "Sensors/sensors.h"
#else
#include "app.h"
#include "crop_stress.h"
#include "config.h"
#include "hal.h"
#include "dht11.h"
#include "board_adc.h"
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
            PRINTF("     Pin         : %s\r\n", dht11_get_active_pin_str());
            PRINTF("     Temperature : ");
            print_float(reading.temperature_c, 1);
            PRINTF(" *C  (");
            print_float(temp_f, 1);
            PRINTF(" *F)\r\n");

            PRINTF("     Humidity    : ");
            print_float(reading.humidity_pct, 1);
            PRINTF(" %%RH\r\n");
        } else {
            PRINTF("     [!] Status  : %s\r\n", dht11_get_status_str());
        }

        /* 2. Soil Moisture (HW-103) */
        const crop_adc_all_pins_t *scan = crop_adc_get_last_scan();
        PRINTF(" 💧 Soil Moisture (J4 Analog Header) :\r\n");
        if (scan != NULL) {
            PRINTF("     J4 Voltage Scan: A0=%u | A1=%u | A2=%u | A3=%u | A4=%u | A5=%u mV\r\n",
                   (unsigned int)scan->a0_mv, (unsigned int)scan->a1_mv, (unsigned int)scan->a2_mv,
                   (unsigned int)scan->a3_mv, (unsigned int)scan->a4_mv, (unsigned int)scan->a5_mv);
            const char *pin_names[] = {"A0 (PTD1)", "A1 (PTD2)", "A2 (PTD3)", "A3 (PTA4)", "A4 (PTB2)", "A5 (PTB3)"};
            PRINTF("     Active Input   : %s\r\n", pin_names[scan->best_pin_idx]);
        }
        if (reading.soil_valid) {
            PRINTF("     Moisture       : ");
            print_float(reading.soil_pct, 1);
            PRINTF(" %%  [Raw: ");
            print_float(reading.soil_raw_mv, 0);
            PRINTF(" mV]\r\n");

            const char *soil_desc = "Optimal";
            if (reading.soil_pct < 15.0f) {
                soil_desc = "CRITICAL DRY (<15%)";
            } else if (reading.soil_pct < 25.0f) {
                soil_desc = "DRY (Needs water)";
            } else if (reading.soil_pct <= 44.0f) {
                soil_desc = "MODERATE DEFICIT (25-44%)";
            } else if (reading.soil_pct <= 75.0f) {
                soil_desc = "OPTIMAL / MOIST";
            } else {
                soil_desc = "VERY WET / Saturated";
            }
            PRINTF("     Condition      : %s\r\n", soil_desc);
            PRINTF("     [Tip: Dip probe in water/wet soil to see voltage & %% jump!]\r\n");
        } else {
            PRINTF("     [!] ADC conversion error\r\n");
        }

        /* 3. Ambient Light */
        PRINTF(" ☀️  Ambient Light (Onboard ALS-PT19) :\r\n");
        PRINTF("     Illuminance : ");
        print_float(reading.light_lux, 0);
        PRINTF(" Lux\r\n");

        /* 4. Edge AI Crop Health Analysis (Stage 3 Calibrated 4-Tier Engine) */
        crop_stress_analysis_t stress = crop_stress_evaluate(&reading);

        PRINTF(" 🤖 Edge AI Crop Health Analysis :\r\n");
        PRINTF("     Stress Score: ");
        print_float(stress.stress_score_pct, 1);
        PRINTF(" %%\r\n");
        PRINTF("     STATUS      : %s\r\n", stress.status_str);
        PRINTF("     ACTION      : %s\r\n", stress.action_str);

        /* 5. Live BLE Telemetry Stream Frame */
        PRINTF(" [BLE-TX] {\"soil\":");
        print_float(reading.soil_pct, 1);
        PRINTF(",\"temp\":");
        print_float(reading.temperature_c, 1);
        PRINTF(",\"hum\":");
        print_float(reading.humidity_pct, 1);
        PRINTF(",\"lux\":");
        print_float(reading.light_lux, 0);
        PRINTF(",\"stress\":");
        print_float(stress.stress_score_pct, 1);
        PRINTF("}\r\n");

        PRINTF("----------------------------------------------------------------\r\n\r\n");

        /* Wait until next sample interval */
        hal_delay_ms(CROP_SAMPLE_INTERVAL_MS);
    }

    return 0;
}
