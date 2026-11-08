/**
 * @file crop_stress.c
 * @brief Implementation of Crop Stress Analysis & Soil LPADC mapping for FRDM-MCXW71.
 */
#if __has_include("APP/crop_stress.h")
#include "APP/crop_stress.h"
#include "Config/config.h"
#else
#include "crop_stress.h"
#include "config.h"
#endif

#include <math.h>
#include <stddef.h>

float crop_stress_raw_mv_to_soil_pct(float raw_mv)
{
    if (!isfinite(raw_mv)) {
        return 0.0f;
    }

    /* HW-103 LPADC (PTD1 / A0) Linear Calibration Mapping:
     * Dry Air Baseline (0% Moisture): Map to 470 mV
     * Saturated / Water Baseline (100% Moisture): Map to 180 mV
     * Formula:
     *   Moisture % = clamp( (470.0 - Raw_mV) / (470.0 - 180.0) * 100.0, 0.0, 100.0 )
     */
    float pct = ((CROP_SOIL_DRY_MV_CALIB - raw_mv) / 
                 (CROP_SOIL_DRY_MV_CALIB - CROP_SOIL_WET_MV_CALIB)) * 100.0f;
    return fmaxf(0.0f, fminf(100.0f, pct));
}

crop_stress_analysis_t crop_stress_evaluate(const crop_reading_t *reading)
{
    crop_stress_analysis_t res;
    if (reading == NULL) {
        res.level = CROP_STRESS_LEVEL_OPTIMAL;
        res.stress_score_pct = 0.0f;
        res.status_str = "[ HEALTHY - Optimal Growth Conditions ]";
        res.action_str = "Sensors idle.";
        return res;
    }

    float soil = reading->soil_pct;
    float temp = reading->temperature_c;
    float hum  = reading->humidity_pct;
    float lux  = reading->light_lux;

    /* -------------------------------------------------------------------------
     * STAGE 3: EDGE AI CROP STRESS THRESHOLD RE-WEIGHTING
     * -------------------------------------------------------------------------
     *
     * 1. CRITICAL STRESS (Score 81% - 100% | Display: "CRITICAL STRESS - Immediate Action Required"):
     *      Soil Moisture < 15% OR (DHT11 Temp > 35°C AND Soil Moisture < 25%)
     */
    if (soil < 15.0f || (temp > 35.0f && soil < 25.0f)) {
        res.level = CROP_STRESS_LEVEL_CRITICAL;
        float deficit = fmaxf(0.0f, 15.0f - soil);
        res.stress_score_pct = 81.0f + (deficit / 15.0f) * 19.0f;
        res.stress_score_pct = fmaxf(81.0f, fminf(100.0f, res.stress_score_pct));
        res.status_str = "CRITICAL STRESS - Immediate Action Required";
        res.action_str = "Irrigate immediately! Root zone moisture is critically exhausted.";
        return res;
    }

    /* 2. HIGH STRESS (Score 51% - 80% | Display: "HIGH STRESS - High Heat / Low Water"):
     *      Soil Moisture between 15% and 24% OR (DHT11 Temp > 32°C with Humidity < 35%)
     */
    if ((soil >= 15.0f && soil < 25.0f) || (temp > 32.0f && hum < 35.0f)) {
        res.level = CROP_STRESS_LEVEL_HIGH;
        float deficit = fmaxf(0.0f, 25.0f - soil);
        res.stress_score_pct = 51.0f + (deficit / 10.0f) * 29.0f;
        res.stress_score_pct = fmaxf(51.0f, fminf(80.0f, res.stress_score_pct));
        res.status_str = "HIGH STRESS - High Heat / Low Water";
        res.action_str = "Priority watering required. Provide shade or cross-ventilation.";
        return res;
    }

    /* 3. MODERATE STRESS (Score 16% - 50% | Display: "MODERATE STRESS - Water Needed"):
     *      Soil Moisture between 25% and 44% OR (Lux > 1000 with Humidity < 45%)
     */
    if ((soil >= 25.0f && soil < 45.0f) || (lux > 1000.0f && hum < 45.0f)) {
        res.level = CROP_STRESS_LEVEL_MODERATE;
        float deficit = fmaxf(0.0f, 45.0f - soil);
        res.stress_score_pct = 16.0f + (deficit / 20.0f) * 34.0f;
        res.stress_score_pct = fmaxf(16.0f, fminf(50.0f, res.stress_score_pct));
        res.status_str = "MODERATE STRESS - Water Needed";
        res.action_str = "Water needed soon. Plan irrigation in current cultivation window.";
        return res;
    }

    /* 4. OPTIMAL (Score 0% - 15% | Display: "[ HEALTHY - Optimal Growth Conditions ]"):
     *      Soil Moisture >= 45% AND DHT11 Temp <= 30°C
     */
    res.level = CROP_STRESS_LEVEL_OPTIMAL;
    float opt_score = 12.0f - (soil - 45.0f) * 0.2f;
    res.stress_score_pct = fmaxf(0.0f, fminf(15.0f, opt_score));
    res.status_str = "[ HEALTHY - Optimal Growth Conditions ]";
    res.action_str = "Maintain current regimen. Plant physiological parameters are optimal.";
    return res;
}
