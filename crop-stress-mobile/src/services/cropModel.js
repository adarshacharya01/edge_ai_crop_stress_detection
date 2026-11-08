/**
 * @file cropModel.js
 * @brief Agronomic Crop Stress Diagnostic & Inference Engine.
 *
 * Implements the calibrated 4-tier Edge AI thresholds:
 *   1. OPTIMAL (0% - 15%): Soil >= 45% AND Temp <= 30°C
 *   2. MODERATE STRESS (16% - 50% | "MODERATE STRESS - Water Needed"):
 *        Soil 25% - 44% OR (Lux > 1000 AND Humidity < 45%)
 *   3. HIGH STRESS (51% - 80% | "HIGH STRESS - High Heat / Low Water"):
 *        Soil 15% - 24% OR (Temp > 32°C AND Humidity < 35%)
 *   4. CRITICAL STRESS (81% - 100% | "CRITICAL STRESS - Immediate Action Required"):
 *        Soil < 15% OR (Temp > 35°C AND Soil < 25%)
 */

export const CROP_ALERT_THRESHOLD = 0.51; // High or Critical stress triggers alert banner

/**
 * Calculates continuous stress probability [0.00 - 1.00]
 * Returns null if no valid reading (disconnected).
 */
export function calculateStressProbability(reading) {
  if (!reading || reading.soil_pct === null || reading.temperature_c === null) {
    return null;
  }

  const { soil_pct, temperature_c, humidity_pct = 50, light_lux = 500 } = reading;

  // 1. Critical Stress Check: Soil < 15% OR (Temp > 35C AND Soil < 25%)
  if (soil_pct < 15.0 || (temperature_c > 35.0 && soil_pct < 25.0)) {
    // Score range: 0.81 - 1.00
    const deficit = Math.max(0, 15.0 - soil_pct);
    const score = 0.81 + (deficit / 15.0) * 0.19;
    return parseFloat(Math.min(1.0, Math.max(0.81, score)).toFixed(2));
  }

  // 2. High Stress Check: Soil 15% - 24% OR (Temp > 32C AND Humidity < 35%)
  if ((soil_pct >= 15.0 && soil_pct < 25.0) || (temperature_c > 32.0 && humidity_pct < 35.0)) {
    // Score range: 0.51 - 0.80
    const deficit = Math.max(0, 25.0 - soil_pct);
    const score = 0.51 + (deficit / 10.0) * 0.29;
    return parseFloat(Math.min(0.80, Math.max(0.51, score)).toFixed(2));
  }

  // 3. Moderate Stress Check: Soil 25% - 44% OR (Lux > 1000 AND Humidity < 45%)
  if ((soil_pct >= 25.0 && soil_pct < 45.0) || (light_lux > 1000.0 && humidity_pct < 45.0)) {
    // Score range: 0.16 - 0.50
    const deficit = Math.max(0, 45.0 - soil_pct);
    const score = 0.16 + (deficit / 20.0) * 0.34;
    return parseFloat(Math.min(0.50, Math.max(0.16, score)).toFixed(2));
  }

  // 4. Optimal: Soil >= 45% AND Temp <= 30C
  // Score range: 0.00 - 0.15
  const baseScore = Math.max(0.02, 0.12 - (soil_pct - 45.0) * 0.002);
  return parseFloat(Math.min(0.15, Math.max(0.01, baseScore)).toFixed(2));
}

/**
 * Diagnoses crop state and returns actionable UI metadata
 */
export function diagnoseCropState(reading, score) {
  if (!reading || score === null || score === undefined) {
    return {
      severity: 'disconnected',
      badge: 'BOARD DISCONNECTED',
      title: 'Board Not Connected - Tap to Connect',
      summary: 'Connect to FRDM-MCXW71 via Bluetooth Low Energy to stream live crop vitals.',
      color: '#94a3b8',
      actions: [
        'Ensure the FRDM-MCXW71 board is powered on and within range.',
        'Tap the Connect button in the header to pair.'
      ]
    };
  }

  const { soil_pct, temperature_c, humidity_pct = 50 } = reading;

  // Tier 4: CRITICAL STRESS (Score 81% - 100%)
  if (score >= 0.81) {
    return {
      severity: 'alert',
      badge: 'CRITICAL STRESS',
      title: 'CRITICAL STRESS - Immediate Action Required',
      summary: soil_pct < 15.0
        ? `Soil moisture is dangerously depleted at ${soil_pct.toFixed(1)}%. Immediate irrigation is necessary to prevent permanent wilting.`
        : `Severe thermal stress (${temperature_c.toFixed(1)}°C) combined with dry root zone (${soil_pct.toFixed(1)}%).`,
      color: '#ef4444',
      actions: [
        'Irrigate crop immediately with deep watering (minimum 25-35mm depth).',
        'Deploy 50% shading cloth to reduce direct radiation load.',
        'Check root zone temperature and inspect for leaf chlorosis.'
      ]
    };
  }

  // Tier 3: HIGH STRESS (Score 51% - 80%)
  if (score >= 0.51) {
    return {
      severity: 'alert',
      badge: 'HIGH STRESS',
      title: 'HIGH STRESS - High Heat / Low Water',
      summary: temperature_c > 32.0 && humidity_pct < 35.0
        ? `Ambient heat is excessive (${temperature_c.toFixed(1)}°C) with dry atmospheric vapor deficit (${humidity_pct.toFixed(1)}% RH).`
        : `Root zone moisture is depressed to ${soil_pct.toFixed(1)}%, restricting transpirational cooling.`,
      color: '#f97316',
      actions: [
        'Schedule priority irrigation within 1-2 hours.',
        'Increase greenhouse cross-ventilation or activate misting.',
        'Verify soil drainage channels and avoid midday fertilizing.'
      ]
    };
  }

  // Tier 2: MODERATE STRESS (Score 16% - 50%)
  if (score >= 0.16) {
    return {
      severity: 'warning',
      badge: 'MODERATE STRESS',
      title: 'MODERATE STRESS - Water Needed',
      summary: soil_pct < 45.0
        ? `Soil moisture is drifting below optimal levels (${soil_pct.toFixed(1)}%). Plant transpires more water than supplied.`
        : `Solar illuminance exceeds 1000 Lux while humidity is dry (${humidity_pct.toFixed(1)}% RH).`,
      color: '#f59e0b',
      actions: [
        'Prepare standard irrigation cycle for the upcoming watering window.',
        'Monitor soil probe levels to confirm root zone absorption.',
        'Maintain good ventilation to moderate canopy microclimate.'
      ]
    };
  }

  // Tier 1: OPTIMAL (Score 0% - 15%)
  return {
    severity: 'optimal',
    badge: 'HEALTHY VITALS',
    title: 'HEALTHY - Optimal Growth Conditions',
    summary: `Soil moisture (${soil_pct.toFixed(1)}%) and air temperature (${temperature_c.toFixed(1)}°C) are well balanced for active photosynthesis.`,
    color: '#10b981',
    actions: [
      'Vitals are optimal; continue standard scheduled irrigation routine.',
      'Sensors operating within physiological sweet-spot.'
    ]
  };
}

/**
 * HW-103 Analog Soil Moisture Voltage-to-Percentage Re-Mapping
 * Linear formula:
 *   Moisture % = clamp( (470.0 - Raw_mV) / (470.0 - 180.0) * 100.0, 0.0, 100.0 )
 */
export function adcMvToSoilPct(rawMv, dryMv = 470.0, wetMv = 180.0) {
  if (rawMv === null || rawMv === undefined || isNaN(rawMv)) return 0;
  if (dryMv <= wetMv) return 0;
  const pct = ((dryMv - rawMv) / (dryMv - wetMv)) * 100.0;
  return Math.max(0.0, Math.min(100.0, pct));
}
