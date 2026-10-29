import React from 'react';
import { Droplets, Thermometer, Wind, Sun } from 'lucide-react';

export default function SensorGrid({ reading }) {
  const { soil_pct, temperature_c, humidity_pct, light_lux } = reading;

  // Visual status indicators
  const getSoilState = (pct) => {
    if (pct < 25) return { label: 'Dry · Water Now', color: '#ef4444' };
    if (pct <= 70) return { label: 'Optimal', color: '#10b981' };
    return { label: 'Saturated', color: '#f59e0b' };
  };

  const getTempState = (temp) => {
    if (temp < 15) return { label: 'Cold Shock', color: '#38bdf8' };
    if (temp <= 28) return { label: 'Ideal', color: '#10b981' };
    return { label: 'Heat Stress', color: '#ef4444' };
  };

  const getHumidityState = (rh) => {
    if (rh < 40) return { label: 'Dry Air', color: '#f59e0b' };
    if (rh <= 75) return { label: 'Comfort', color: '#10b981' };
    return { label: 'High Humidity', color: '#38bdf8' };
  };

  const getLightState = (lux) => {
    if (lux < 2000) return { label: 'Low Shade', color: '#94a3b8' };
    if (lux <= 32000) return { label: 'Optimal Sun', color: '#10b981' };
    return { label: 'Direct Scorch', color: '#f59e0b' };
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
          {soil_pct.toFixed(1)}<small>%</small>
        </div>
        <div className="sensor-mini-bar">
          <div
            className="sensor-mini-bar-fill"
            style={{ width: `${Math.min(100, Math.max(0, soil_pct))}%`, background: soil.color }}
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
          {temperature_c.toFixed(1)}<small>°C</small>
        </div>
        <div className="sensor-mini-bar">
          <div
            className="sensor-mini-bar-fill"
            style={{ width: `${Math.min(100, Math.max(0, (temperature_c / 45) * 100))}%`, background: temp.color }}
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
          {humidity_pct.toFixed(1)}<small>%</small>
        </div>
        <div className="sensor-mini-bar">
          <div
            className="sensor-mini-bar-fill"
            style={{ width: `${Math.min(100, Math.max(0, humidity_pct))}%`, background: rh.color }}
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
          {light_lux >= 1000 ? `${(light_lux / 1000).toFixed(1)}k` : light_lux.toFixed(0)}<small>lx</small>
        </div>
        <div className="sensor-mini-bar">
          <div
            className="sensor-mini-bar-fill"
            style={{ width: `${Math.min(100, Math.max(0, (light_lux / 50000) * 100))}%`, background: light.color }}
          />
        </div>
      </div>
    </div>
  );
}
