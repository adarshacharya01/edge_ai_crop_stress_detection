/**
 * @file cropModel.js
 * @brief Agronomic Crop Stress Diagnostic & Inference Engine.
 *
 * Implements the balanced 4-tier Edge AI thresholds:
 *   1. OPTIMAL (0% - 25%): Soil 50% - 80% AND Temp 20°C - 27°C
 *   2. MODERATE STRESS (26% - 55% | "MODERATE STRESS - Water Needed"):
 *        Soil deficit (in dry air ~40% stress) under normal room temp
 *   3. HIGH STRESS (56% - 79% | "HIGH STRESS - High Heat / Water Needed"):
 *        Soil dry + warm temperature (>30°C) or dry air
 *   4. CRITICAL STRESS (80% - 100% | "CRITICAL STRESS - Immediate Action Required"):
 *        Extreme compound stress (Soil < 10% AND Temp > 34°C)
 */

export const CROP_ALERT_THRESHOLD = 0.56; // High or Critical stress triggers alert banner

/**
 * Calculates continuous stress probability [0.00 - 1.00]
 * Balanced Multi-Factor Crop Stress Engine (Medium-Tier Weighting):
 *   - Dry soil in normal room temp (~24°C) evaluates to ~0.40 (MODERATE STRESS)
 *   - Healthy moist soil evaluates to ~0.05 - 0.15 (OPTIMAL)
 *   - Compound heat (>30°C) + dry soil scales to HIGH (0.56 - 0.79)
 *   - Extreme drought (<10%) + severe heat (>35°C) scales to CRITICAL (0.80 - 1.00)
 */
export function calculateStressProbability(reading) {
  if (!reading || reading.soil_pct === null || reading.temperature_c === null) {
    return null;
  }

  const { soil_pct, temperature_c, humidity_pct = 50, light_lux = 500 } = reading;

  // 1. Soil Deficit Base Stress (0.00 - 0.40)
  let soilStress = 0.0;
  if (soil_pct < 50.0) {
    soilStress = ((50.0 - soil_pct) / 50.0) * 0.40;
  }

  // 2. Heat Stress (0.00 - 0.35)
  let tempStress = 0.0;
  if (temperature_c > 28.0) {
    tempStress = Math.min(0.35, (temperature_c - 28.0) * 0.035);
  }

  // 3. Low Humidity Stress (0.00 - 0.15)
  let humStress = 0.0;
  if (humidity_pct < 40.0 && temperature_c > 26.0) {
    humStress = ((40.0 - humidity_pct) / 40.0) * 0.15;
  }

  // 4. Excessive Illuminance Stress (0.00 - 0.10)
  let lightStress = 0.0;
  if (light_lux > 1500.0 && temperature_c > 30.0) {
    lightStress = 0.10;
  }

  const total = Math.min(1.0, Math.max(0.0, soilStress + tempStress + humStress + lightStress));
  return parseFloat(total.toFixed(2));
}

/**
 * Diagnoses crop state and returns actionable UI metadata
 */
export function diagnoseCropState(reading, score, isConnected = false) {
  if (!isConnected) {
    return {
      severity: 'disconnected',
      badge: 'BOARD DISCONNECTED',
      title: 'Board Not Connected - Tap to Connect',
      summary: 'Connect to FRDM-MCXW71 via Bluetooth Low Energy to stream live crop vitals.',
      color: '#94a3b8',
      actions: [
        'Ensure the FRDM-MCXW71 board is powered on and within range.',
        'Tap the Connect Sensor button in the header to pair.'
      ]
    };
  }

  if (!reading || score === null || score === undefined) {
    return {
      severity: 'connected',
      badge: 'CONNECTED · LIVE',
      title: 'FRDM-MCXW71 Connected',
      summary: 'Streaming real-time telemetry notifications (2s cadence)...',
      color: '#10b981',
      actions: [
        'Hardware link active and streaming notifications.',
        'Receiving multi-sensor telemetry from Soil LPADC & DHT11.'
      ]
    };
  }

  const { soil_pct, temperature_c, humidity_pct = 50 } = reading;

  // Tier 4: CRITICAL STRESS (Score 80% - 100%)
  if (score >= 0.80) {
    return {
      severity: 'alert',
      badge: 'CRITICAL STRESS',
      title: 'CRITICAL STRESS - Immediate Action Required',
      summary: `Severe compound stress: High temperature (${temperature_c.toFixed(1)}°C) combined with exhausted soil moisture (${soil_pct.toFixed(1)}%).`,
      color: '#ef4444',
      actions: [
        'Irrigate crop immediately with deep watering (minimum 25-35mm depth).',
        'Deploy 50% shading cloth to reduce direct radiation load.',
        'Check root zone temperature and inspect for leaf chlorosis.'
      ]
    };
  }

  // Tier 3: HIGH STRESS (Score 56% - 79%)
  if (score >= 0.56) {
    return {
      severity: 'alert',
      badge: 'HIGH STRESS',
      title: 'HIGH STRESS - High Heat / Water Needed',
      summary: `Elevated thermal stress (${temperature_c.toFixed(1)}°C) and low root zone moisture (${soil_pct.toFixed(1)}%).`,
      color: '#f97316',
      actions: [
        'Schedule priority irrigation within 1-2 hours.',
        'Increase greenhouse cross-ventilation or activate misting.',
        'Verify soil drainage channels and avoid midday fertilizing.'
      ]
    };
  }

  // Tier 2: MODERATE STRESS (Score 26% - 55%)
  if (score >= 0.26) {
    return {
      severity: 'warning',
      badge: 'MODERATE STRESS',
      title: 'MODERATE STRESS - Water Needed',
      summary: `Soil moisture is dry (${soil_pct.toFixed(1)}%). Ambient temperature is manageable at ${temperature_c.toFixed(1)}°C.`,
      color: '#f59e0b',
      actions: [
        'Prepare standard irrigation cycle for upcoming watering window.',
        'Monitor soil probe levels to confirm root zone absorption.',
        'Maintain good ventilation to moderate canopy microclimate.'
      ]
    };
  }

  // Tier 1: OPTIMAL (Score 0% - 25%)
  return {
    severity: 'optimal',
    badge: 'HEALTHY VITALS',
    title: 'HEALTHY - Optimal Growth Conditions',
    summary: `Soil moisture (${soil_pct.toFixed(1)}%) and air temperature (${temperature_c.toFixed(1)}°C) are well balanced for active growth.`,
    color: '#10b981',
    actions: [
      'Vitals are optimal; continue standard scheduled irrigation routine.',
      'Sensors operating within physiological sweet-spot.'
    ]
  };
}

/**
 * HW-103 Analog Soil Moisture Voltage-to-Percentage Re-Mapping
 * Full dry in air (>= 475 mV) -> 0.0%
 * Saturated in water (<= 180 mV) -> 100.0%
 */
export function adcMvToSoilPct(rawMv, dryMv = 475.0, wetMv = 180.0) {
  if (rawMv === null || rawMv === undefined || isNaN(rawMv)) return 0;
  if (rawMv >= dryMv) return 0.0;
  if (rawMv <= wetMv) return 100.0;
  const pct = ((dryMv - rawMv) / (dryMv - wetMv)) * 100.0;
  return Math.max(0.0, Math.min(100.0, pct));
}
