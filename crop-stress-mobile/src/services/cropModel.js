// Edge AI crop stress inference model & diagnostic engine

/**
 * Calculates stress probability mimicking the embedded C model (Model/model.c)
 * Inputs:
 * - soil_pct: 0 - 100 (%)
 * - temperature_c: in Celsius
 * - humidity_pct: 0 - 100 (%)
 * - light_lux: 0 - 100,000+ (lux)
 */
export function calculateStressProbability({ soil_pct, temperature_c, humidity_pct, light_lux }) {
  // Edge AI Feature Transformation: Log-transform for wide dynamic range light
  const logLux = Math.log1p(Math.max(0, light_lux));

  // Domain knowledge baseline weights reflecting plant physiology
  // Soil moisture: Low soil (< 30%) sharply increases stress
  const soilFactor = (45 - soil_pct) * 0.08; 

  // Temperature: Optimal is 20 - 26 C. Deviations above 30C or below 14C drive stress
  let tempFactor = 0;
  if (temperature_c > 26) {
    tempFactor = (temperature_c - 26) * 0.18;
  } else if (temperature_c < 16) {
    tempFactor = (16 - temperature_c) * 0.15;
  }

  // Humidity: Optimal is 50 - 70%. Very low (< 35%) causes vapor deficit stress
  let rhFactor = 0;
  if (humidity_pct < 45) {
    rhFactor = (45 - humidity_pct) * 0.06;
  } else if (humidity_pct > 85) {
    rhFactor = (humidity_pct - 85) * 0.04; // Saturated air risk
  }

  // Light factor: high light + low moisture = acute photo-inhibition / scorch
  let lightFactor = 0;
  if (logLux > 9.5 && soil_pct < 35) {
    lightFactor = (logLux - 9.5) * 0.4;
  }

  // Compute logit (log-odds)
  const logit = -1.8 + soilFactor + tempFactor + rhFactor + lightFactor;

  // Clamped Sigmoid function 1 / (1 + e^-logit)
  const clampedLogit = Math.max(-15, Math.min(15, logit));
  const probability = 1 / (1 + Math.exp(-clampedLogit));

  return parseFloat(probability.toFixed(3));
}

export const CROP_ALERT_THRESHOLD = 0.70; // Matches Config/config.h CROP_ALERT_PROBABILITY

/**
 * Provides agronomic diagnosis and prioritized action recommendations
 */
export function diagnoseCropState(reading, probability) {
  const { soil_pct, temperature_c, humidity_pct, light_lux } = reading;

  if (probability >= CROP_ALERT_THRESHOLD) {
    // Critical Alert Cases
    if (soil_pct < 25 && temperature_c > 30) {
      return {
        severity: 'alert',
        badge: 'CRITICAL STRESS',
        title: 'Severe Drought & Heatwave Stress',
        summary: 'Soil moisture is depleted under elevated temperatures, risking stomatal closure and wilting.',
        color: '#ef4444',
        actions: [
          'Irrigate crop immediately (recommend 20-30mm depth).',
          'Deploy shade netting to lower direct solar radiation.',
          'Schedule misting if greenhouse ventilation allows.'
        ]
      };
    } else if (soil_pct > 88 && humidity_pct > 80) {
      return {
        severity: 'alert',
        badge: 'OVERWATERING ALERT',
        title: 'Hypoxia & Root Rot Hazard',
        summary: 'Root zone is waterlogged while humidity is saturated, depriving roots of oxygen.',
        color: '#f97316',
        actions: [
          'Immediately cease all automated irrigation.',
          'Verify drainage channels are unobstructed.',
          'Improve airflow ventilation to lower ambient humidity.'
        ]
      };
    } else if (temperature_c < 10) {
      return {
        severity: 'alert',
        badge: 'CHILL / FROST RISK',
        title: 'Cold Shock & Stunted Growth',
        summary: 'Ambient temperature is below physiological tolerance for metabolic activity.',
        color: '#38bdf8',
        actions: [
          'Activate greenhouse heating or thermal row covers.',
          'Avoid wet soil during cold spells to prevent root damage.'
        ]
      };
    } else {
      return {
        severity: 'alert',
        badge: 'STRESS ALERT',
        title: 'High Cumulative Plant Stress',
        summary: 'Combined environmental metrics exceed the safety threshold. Plant requires inspection.',
        color: '#ef4444',
        actions: [
          'Perform physical plant inspection for curling or leaf drooping.',
          'Adjust root-zone water balance to target 45-65% moisture.'
        ]
      };
    }
  } else if (probability >= 0.40) {
    // Moderate warning
    return {
      severity: 'warning',
      badge: 'MODERATE STRESS',
      title: 'Sub-Optimal Vitals Detected',
      summary: 'Environmental balance is beginning to drift from optimal comfort ranges.',
      color: '#f59e0b',
      actions: [
        soil_pct < 35 ? 'Plan soil watering within the next 4-6 hours.' : 'Monitor soil moisture.',
        temperature_c > 28 ? 'Increase natural cross-ventilation.' : 'Check ambient airflow.'
      ]
    };
  } else {
    // Optimal / Normal
    return {
      severity: 'optimal',
      badge: 'HEALTHY VITALS',
      title: 'Optimal Growing Condition',
      summary: 'All telemetry metrics align with healthy metabolic photosynthesis and hydration.',
      color: '#10b981',
      actions: [
        'Vitals are stable; maintain current irrigation cycle.',
        'Sensors are operating within ideal limits.'
      ]
    };
  }
}

/**
 * Converts analog ADC millivolts to calibrated soil percentage
 * Matches Sensors/sensors.c formula
 */
export function adcMvToSoilPct(rawMv, dryMv = 2600, wetMv = 1100) {
  if (dryMv <= wetMv) return 0;
  const pct = ((dryMv - rawMv) / (dryMv - wetMv)) * 100.0;
  return Math.min(100.0, Math.max(0.0, parseFloat(pct.toFixed(1))));
}
