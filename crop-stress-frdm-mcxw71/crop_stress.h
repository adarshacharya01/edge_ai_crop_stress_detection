/**
 * @file crop_stress.h
 * @brief Calibrated Crop Stress Analysis Engine and Soil LPADC Mapping for FRDM-MCXW71.
 */
#ifndef CROP_STRESS_H
#define CROP_STRESS_H

#include <stdint.h>
#include <stdbool.h>

#ifndef CROP_READING_TYPE_DEFINED
#define CROP_READING_TYPE_DEFINED
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
#endif

/* HW-103 Soil Probe Voltage Calibration Baselines (mV) */
#define CROP_SOIL_DRY_MV_CALIB   475.0f  /* Dry air baseline (0% moisture): 475 mV */
#define CROP_SOIL_WET_MV_CALIB   180.0f  /* Saturated water baseline (100% moisture): 180 mV */

/* 4-Tier Crop Stress Classification */
typedef enum {
    CROP_STRESS_LEVEL_OPTIMAL = 0,
    CROP_STRESS_LEVEL_MODERATE,
    CROP_STRESS_LEVEL_HIGH,
    CROP_STRESS_LEVEL_CRITICAL
} crop_stress_level_t;

typedef struct {
    crop_stress_level_t level;
    float stress_score_pct;      /* 0.0% to 100.0% */
    const char *status_str;      /* Exact display string */
    const char *action_str;      /* Action guidance */
} crop_stress_analysis_t;

/**
 * @brief Linear mapping formula for HW-103 probe on LPADC0 (PTD1 / A0):
 *        Moisture % = clamp( (470.0 - Raw_mV) / (470.0 - 180.0) * 100.0, 0.0, 100.0 )
 *
 * @param raw_mv Raw millivolt reading from LPADC0.
 * @return Calibrated moisture percentage [0.0% - 100.0%].
 */
float crop_stress_raw_mv_to_soil_pct(float raw_mv);

/**
 * @brief Evaluate 4-tier Crop Stress conditions from telemetry readings.
 *
 * Criteria:
 *   - CRITICAL STRESS (Score 81% - 100% | "CRITICAL STRESS - Immediate Action Required"):
 *       Soil < 15% OR (Temp > 35°C AND Soil < 25%)
 *   - HIGH STRESS (Score 51% - 80% | "HIGH STRESS - High Heat / Low Water"):
 *       Soil 15% - 24% OR (Temp > 32°C AND Humidity < 35%)
 *   - MODERATE STRESS (Score 16% - 50% | "MODERATE STRESS - Water Needed"):
 *       Soil 25% - 44% OR (Lux > 1000 AND Humidity < 45%)
 *   - OPTIMAL (Score 0% - 15% | "[ HEALTHY - Optimal Growth Conditions ]"):
 *       Soil >= 45% AND Temp <= 30°C
 *
 * @param reading Pointer to current sensor telemetry reading.
 * @return Analysis result with level, score percentage, status string, and action text.
 */
crop_stress_analysis_t crop_stress_evaluate(const crop_reading_t *reading);

#endif /* CROP_STRESS_H */

