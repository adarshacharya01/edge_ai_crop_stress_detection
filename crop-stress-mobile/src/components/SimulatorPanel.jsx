import React, { useState, useEffect } from 'react';
import { Play, Pause, Droplets } from 'lucide-react';
import { calculateStressProbability, CROP_ALERT_THRESHOLD } from '../services/cropModel';

const PRESETS = [
  {
    name: '🌿 Healthy',
    soil: 52,
    temp: 23.5,
    rh: 62,
    lux: 18000,
  },
  {
    name: '💧 Dry Soil',
    soil: 16,
    temp: 29.0,
    rh: 35,
    lux: 25000,
  },
  {
    name: '🔥 Heatwave',
    soil: 14,
    temp: 37.5,
    rh: 22,
    lux: 52000,
  },
  {
    name: '🌊 Overwatered',
    soil: 94,
    temp: 18.0,
    rh: 88,
    lux: 1500,
  },
  {
    name: '❄️ Cold Shock',
    soil: 40,
    temp: 7.0,
    rh: 48,
    lux: 4000,
  }
];

export default function SimulatorPanel({ reading, onUpdateReading }) {
  const [activePreset, setActivePreset] = useState('🌿 Healthy');
  const [isAutoCycling, setIsAutoCycling] = useState(false);

  // Auto-simulation timer: mimics real plant drying and sun rising
  useEffect(() => {
    if (!isAutoCycling) return;
    const interval = setInterval(() => {
      onUpdateReading((prev) => {
        const newSoil = Math.max(8, prev.soil_pct - 0.4);
        const tempDelta = (Math.random() - 0.45) * 0.3;
        const newTemp = Math.min(42, Math.max(10, prev.temperature_c + tempDelta));
        const newRh = Math.min(95, Math.max(15, prev.humidity_pct + (Math.random() - 0.5) * 0.6));
        const newLux = Math.min(65000, Math.max(500, prev.light_lux + (Math.random() - 0.48) * 500));

        return {
          soil_pct: parseFloat(newSoil.toFixed(1)),
          temperature_c: parseFloat(newTemp.toFixed(1)),
          humidity_pct: parseFloat(newRh.toFixed(1)),
          light_lux: Math.round(newLux)
        };
      });
    }, 2000);

    return () => clearInterval(interval);
  }, [isAutoCycling, onUpdateReading]);

  const applyPreset = (preset) => {
    setActivePreset(preset.name);
    onUpdateReading({
      soil_pct: preset.soil,
      temperature_c: preset.temp,
      humidity_pct: preset.rh,
      light_lux: preset.lux
    });
  };

  const handleWater = () => {
    onUpdateReading((prev) => ({
      ...prev,
      soil_pct: Math.min(90, Math.max(20, prev.soil_pct + 35))
    }));
  };

  const currentScore = calculateStressProbability(reading);
  const isAlert = currentScore >= CROP_ALERT_THRESHOLD;
  const isWarning = currentScore >= 0.40 && !isAlert;
  const scoreColor = isAlert ? '#ef4444' : isWarning ? '#f59e0b' : '#10b981';

  return (
    <div className="sim-panel-container" id="simulator-test-bench">
      {/* Live AI Response Preview Bar */}
      <div className="sim-response-banner" style={{ borderColor: `${scoreColor}40` }}>
        <div className="sim-response-left">
          <div className="sim-score-badge" style={{ color: scoreColor, background: `${scoreColor}18` }}>
            {(currentScore * 100).toFixed(0)}% STRESS
          </div>
          <div className="sim-response-text">
            {isAlert ? '⚠️ Alert Triggered' : isWarning ? '⚡ Sub-optimal Vitals' : '✓ Normal Vitals'}
          </div>
        </div>
        <button
          className="sim-water-btn"
          onClick={handleWater}
          id="btn-quick-water"
          title="Simulate Watering"
        >
          <Droplets size={14} />
          <span>Water (+35%)</span>
        </button>
      </div>

      {/* Preset Pills */}
      <div className="sim-section-header">
        <span className="sim-section-title">Scenario Presets</span>
        <span style={{ fontSize: '0.72rem', color: 'var(--text-dim)' }}>Tap to test condition</span>
      </div>

      <div className="preset-scroll-track">
        {PRESETS.map((p) => (
          <button
            key={p.name}
            className={`preset-chip ${activePreset === p.name ? 'active' : ''}`}
            onClick={() => applyPreset(p)}
          >
            {p.name}
          </button>
        ))}
      </div>

      {/* Tuning Sliders */}
      <div className="sim-slider-list">
        {/* Soil Moisture Slider */}
        <div className="sim-slider-card">
          <div className="sim-slider-top">
            <span className="sim-slider-label">Soil Moisture</span>
            <span className="sim-slider-val" style={{ color: reading.soil_pct < 25 ? '#ef4444' : '#38bdf8' }}>
              {reading.soil_pct}%
            </span>
          </div>
          <input
            type="range"
            min="0"
            max="100"
            step="1"
            value={reading.soil_pct}
            onChange={(e) => {
              setActivePreset('Custom');
              onUpdateReading((prev) => ({ ...prev, soil_pct: parseFloat(e.target.value) }));
            }}
          />
        </div>

        {/* Temperature Slider */}
        <div className="sim-slider-card">
          <div className="sim-slider-top">
            <span className="sim-slider-label">Air Temperature</span>
            <span className="sim-slider-val" style={{ color: reading.temperature_c > 30 ? '#ef4444' : '#10b981' }}>
              {reading.temperature_c}°C
            </span>
          </div>
          <input
            type="range"
            min="5"
            max="45"
            step="0.5"
            value={reading.temperature_c}
            onChange={(e) => {
              setActivePreset('Custom');
              onUpdateReading((prev) => ({ ...prev, temperature_c: parseFloat(e.target.value) }));
            }}
          />
        </div>

        {/* Humidity Slider */}
        <div className="sim-slider-card">
          <div className="sim-slider-top">
            <span className="sim-slider-label">Relative Humidity</span>
            <span className="sim-slider-val" style={{ color: '#34d399' }}>
              {reading.humidity_pct}%
            </span>
          </div>
          <input
            type="range"
            min="10"
            max="100"
            step="1"
            value={reading.humidity_pct}
            onChange={(e) => {
              setActivePreset('Custom');
              onUpdateReading((prev) => ({ ...prev, humidity_pct: parseFloat(e.target.value) }));
            }}
          />
        </div>

        {/* Sunlight Lux Slider */}
        <div className="sim-slider-card">
          <div className="sim-slider-top">
            <span className="sim-slider-label">Sunlight</span>
            <span className="sim-slider-val" style={{ color: '#fbbf24' }}>
              {reading.light_lux.toLocaleString()} lx
            </span>
          </div>
          <input
            type="range"
            min="100"
            max="70000"
            step="500"
            value={reading.light_lux}
            onChange={(e) => {
              setActivePreset('Custom');
              onUpdateReading((prev) => ({ ...prev, light_lux: parseInt(e.target.value, 10) }));
            }}
          />
        </div>
      </div>

      {/* Auto cycle simulation button */}
      <div style={{ marginTop: 6 }}>
        <button
          className={`sim-toggle-btn ${isAutoCycling ? 'running' : ''}`}
          onClick={() => setIsAutoCycling(!isAutoCycling)}
          id="btn-auto-simulate"
        >
          {isAutoCycling ? <Pause size={15} /> : <Play size={15} />}
          <span>{isAutoCycling ? 'Stop Dynamic Day Cycle' : 'Run Dynamic Day Cycle'}</span>
        </button>
      </div>
    </div>
  );
}
