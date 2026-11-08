import React from 'react';
import { Droplets, Thermometer, Wind, Sun } from 'lucide-react';

export default function SensorGrid({ 
  reading, 
  isConnected: bleConnected,
  soilMoisture = null,
  temperature = null,
  humidity = null,
  luxValue = null 
}) {
  const soil_pct = (soilMoisture !== null && soilMoisture !== undefined) 
    ? Number(soilMoisture) 
    : (reading?.soil_pct ?? null);
  const temperature_c = (temperature !== null && temperature !== undefined) 
    ? Number(temperature) 
    : (reading?.temperature_c ?? null);
  const humidity_pct = (humidity !== null && humidity !== undefined) 
    ? Number(humidity) 
    : (reading?.humidity_pct ?? null);
  const light_lux = (luxValue !== null && luxValue !== undefined) 
    ? Number(luxValue) 
    : (reading?.light_lux ?? null);

  // Visual status indicators
  const getSoilState = (pct) => {
    if (pct === null) return bleConnected ? { label: 'Live / Normal', color: '#10b981' } : { label: 'Not Connected', color: '#64748b' };
    if (pct < 15) return { label: 'Critical Dry', color: '#ef4444' };
    if (pct < 25) return { label: 'Dry · Water Now', color: '#f97316' };
    if (pct < 45) return { label: 'Moderate Deficit', color: '#f59e0b' };
    if (pct <= 75) return { label: 'Optimal', color: '#10b981' };
    return { label: 'Saturated', color: '#38bdf8' };
  };

  const getTempState = (temp) => {
    if (temp === null) return bleConnected ? { label: 'Live / Normal', color: '#10b981' } : { label: 'Not Connected', color: '#64748b' };
    if (temp < 15) return { label: 'Cold Shock', color: '#38bdf8' };
    if (temp <= 30) return { label: 'Ideal', color: '#10b981' };
    if (temp <= 32) return { label: 'Elevated', color: '#f59e0b' };
    if (temp <= 35) return { label: 'High Heat', color: '#f97316' };
    return { label: 'Extreme Heat', color: '#ef4444' };
  };

  const getHumidityState = (rh) => {
    if (rh === null) return bleConnected ? { label: 'Live / Normal', color: '#10b981' } : { label: 'Not Connected', color: '#64748b' };
    if (rh < 35) return { label: 'Extreme Dry Air', color: '#ef4444' };
    if (rh < 45) return { label: 'Dry Air', color: '#f59e0b' };
    if (rh <= 75) return { label: 'Comfort', color: '#10b981' };
    return { label: 'High Humidity', color: '#38bdf8' };
  };

  const getLightState = (lux) => {
    if (lux === null) return bleConnected ? { label: 'Live / Normal', color: '#10b981' } : { label: 'Not Connected', color: '#64748b' };
    if (lux < 500) return { label: 'Deep Shade', color: '#94a3b8' };
    if (lux <= 1000) return { label: 'Mild Light', color: '#38bdf8' };
    if (lux <= 32000) return { label: 'Optimal Sun', color: '#10b981' };
    return { label: 'Intense Sun', color: '#f59e0b' };
  };

  const soil = getSoilState(soil_pct);
  const temp = getTempState(temperature_c);
  const rh = getHumidityState(humidity_pct);
  const light = getLightState(light_lux);

  return (
    <div className="sensor-grid" id="sensor-telemetry-grid">
      {/* 1. Soil Moisture */}
      <div className="sensor-card">
        <div className="sensor-card-header">
          <div className="sensor-card-left">
            <div className="sensor-icon-wrap" style={{ background: 'rgba(14, 165, 233, 0.12)', color: '#38bdf8' }}>
              <Droplets size={15} />
            </div>
            <span className="sensor-card-title">Soil Moisture</span>
          </div>
          <span className="sensor-micro-pill" style={{ color: soil.color, background: `${soil.color}15` }}>
            {soil.label}
          </span>
        </div>
        <div className="sensor-value">
          {soil_pct !== null ? (
            <>
              {soil_pct.toFixed(1)}<small>%</small>
            </>
          ) : (
            <span>--</span>
          )}
        </div>
        <div className="sensor-mini-bar">
          <div
            className="sensor-mini-bar-fill"
            style={{ width: `${soil_pct !== null ? Math.min(100, Math.max(0, soil_pct)) : 0}%`, background: soil.color }}
          />
        </div>
      </div>

      {/* 2. Air Temperature */}
      <div className="sensor-card">
        <div className="sensor-card-header">
          <div className="sensor-card-left">
            <div className="sensor-icon-wrap" style={{ background: 'rgba(239, 68, 68, 0.12)', color: '#f87171' }}>
              <Thermometer size={15} />
            </div>
            <span className="sensor-card-title">Air Temp</span>
          </div>
          <span className="sensor-micro-pill" style={{ color: temp.color, background: `${temp.color}15` }}>
            {temp.label}
          </span>
        </div>
        <div className="sensor-value">
          {temperature_c !== null ? (
            <>
              {temperature_c.toFixed(1)}<small>°C</small>
            </>
          ) : (
            <span>--</span>
          )}
        </div>
        <div className="sensor-mini-bar">
          <div
            className="sensor-mini-bar-fill"
            style={{ width: `${temperature_c !== null ? Math.min(100, Math.max(0, (temperature_c / 45) * 100)) : 0}%`, background: temp.color }}
          />
        </div>
      </div>

      {/* 3. Relative Humidity */}
      <div className="sensor-card">
        <div className="sensor-card-header">
          <div className="sensor-card-left">
            <div className="sensor-icon-wrap" style={{ background: 'rgba(16, 185, 129, 0.12)', color: '#34d399' }}>
              <Wind size={15} />
            </div>
            <span className="sensor-card-title">Humidity</span>
          </div>
          <span className="sensor-micro-pill" style={{ color: rh.color, background: `${rh.color}15` }}>
            {rh.label}
          </span>
        </div>
        <div className="sensor-value">
          {humidity_pct !== null ? (
            <>
              {humidity_pct.toFixed(1)}<small>%</small>
            </>
          ) : (
            <span>--</span>
          )}
        </div>
        <div className="sensor-mini-bar">
          <div
            className="sensor-mini-bar-fill"
            style={{ width: `${humidity_pct !== null ? Math.min(100, Math.max(0, humidity_pct)) : 0}%`, background: rh.color }}
          />
        </div>
      </div>

      {/* 4. Sunlight */}
      <div className="sensor-card">
        <div className="sensor-card-header">
          <div className="sensor-card-left">
            <div className="sensor-icon-wrap" style={{ background: 'rgba(245, 158, 11, 0.12)', color: '#fbbf24' }}>
              <Sun size={15} />
            </div>
            <span className="sensor-card-title">Sunlight</span>
          </div>
          <span className="sensor-micro-pill" style={{ color: light.color, background: `${light.color}15` }}>
            {light.label}
          </span>
        </div>
        <div className="sensor-value">
          {light_lux !== null ? (
            <>
              {light_lux >= 1000 ? `${(light_lux / 1000).toFixed(1)}k` : light_lux.toFixed(0)}<small>lx</small>
            </>
          ) : (
            <span>--</span>
          )}
        </div>
        <div className="sensor-mini-bar">
          <div
            className="sensor-mini-bar-fill"
            style={{ width: `${light_lux !== null ? Math.min(100, Math.max(0, (light_lux / 50000) * 100)) : 0}%`, background: light.color }}
          />
        </div>
      </div>
    </div>
  );
}
