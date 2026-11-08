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

    /* HW-103 Soil Probe Linear Calibration Mapping:
     * Full Dry Air Baseline (0% Moisture): Map to 475 mV (or higher in air)
     * Full Saturated Water Baseline (100% Moisture): Map to 180 mV
     */
    if (raw_mv >= CROP_SOIL_DRY_MV_CALIB) {
        return 0.0f;
    }
    if (raw_mv <= CROP_SOIL_WET_MV_CALIB) {
        return 100.0f;
    }

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

    float soil = reading->soil_pct;          /* 0.0% to 100.0% */
    float temp = reading->temperature_c;     /* Celsius */
    float hum  = reading->humidity_pct;      /* %RH */
    float lux  = reading->light_lux;         /* Lux */

    /* -------------------------------------------------------------------------
     * BALANCED MULTI-FACTOR CROP STRESS ENGINE (MEDIUM-TIER WEIGHTING)
     * -------------------------------------------------------------------------
     * 1. Base Soil Moisture Deficit Stress (0.0% to 40.0%):
     *    Ideal soil moisture is 50% - 80%.
     *    When dry in air (soil = 0%), base water stress is 40.0% (Medium alert).
     *    When soil >= 50%, soil stress is 0.0%.
     */
    float soil_stress = 0.0f;
    if (soil < 50.0f) {
        soil_stress = ((50.0f - soil) / 50.0f) * 40.0f;
    }

    /* 2. Temperature Heat Stress (0.0% to 35.0%):
     *    Optimal growth range: 20°C - 27°C.
     *    Heat stress starts above 28°C.
     */
    float temp_stress = 0.0f;
    if (temp > 28.0f) {
        temp_stress = (temp - 28.0f) * 3.5f;
        if (temp_stress > 35.0f) {
            temp_stress = 35.0f;
        }
    }

    /* 3. Humidity / Vapor Pressure Deficit (0.0% to 15.0%):
     *    Dry ambient air (<40% RH) with warm temperatures accelerates transpiration.
     */
    float hum_stress = 0.0f;
    if (hum < 40.0f && temp > 26.0f) {
        hum_stress = ((40.0f - hum) / 40.0f) * 15.0f;
    }

    /* 4. Excessive Illuminance Stress (0.0% to 10.0%):
     *    Intense direct sunlight (>1500 Lux) with warmth increases thermal load.
     */
    float light_stress = 0.0f;
    if (lux > 1500.0f && temp > 30.0f) {
        light_stress = 10.0f;
    }

    /* Compute Total Stress Score (0.0% - 100.0%) */
    float total_stress = soil_stress + temp_stress + hum_stress + light_stress;
    if (total_stress > 100.0f) total_stress = 100.0f;
    if (total_stress < 0.0f)   total_stress = 0.0f;

    res.stress_score_pct = total_stress;

    /* -------------------------------------------------------------------------
     * BALANCED CLASSIFICATION TIERS:
     *   - OPTIMAL (0% - 25%)      : Healthy, optimal conditions.
     *   - MODERATE (26% - 55%)    : Water needed; dry soil in normal room temp.
     *   - HIGH (56% - 79%)        : High heat + dry soil; irrigation priority.
     *   - CRITICAL (80% - 100%)   : Severe drought + extreme heat (>35°C).
     * ------------------------------------------------------------------------- */
    if (total_stress >= 80.0f) {
        res.level = CROP_STRESS_LEVEL_CRITICAL;
        res.status_str = "CRITICAL STRESS - Immediate Action Required";
        res.action_str = "Irrigate immediately! Compound heat and drought exhaustion.";
    } else if (total_stress >= 56.0f) {
        res.level = CROP_STRESS_LEVEL_HIGH;
        res.status_str = "HIGH STRESS - High Heat / Water Needed";
        res.action_str = "Priority watering required. Provide shade or ventilation.";
    } else if (total_stress >= 26.0f) {
        res.level = CROP_STRESS_LEVEL_MODERATE;
        res.status_str = "MODERATE STRESS - Water Needed";
        res.action_str = "Soil is dry. Plan irrigation in next routine watering window.";
    } else {
        res.level = CROP_STRESS_LEVEL_OPTIMAL;
        res.status_str = "[ HEALTHY - Optimal Growth Conditions ]";
        res.action_str = "Maintain current regimen. Plant physiological parameters are healthy.";
    }

    return res;
}

