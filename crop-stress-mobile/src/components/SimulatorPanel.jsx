import React, { useState, useEffect } from 'react';
import { Sliders, Play, Pause, Sparkles, Droplet } from 'lucide-react';

const PRESETS = [
  {
    name: '🌿 Healthy',
    soil: 52,
    temp: 23.5,
    rh: 62,
    lux: 15000,
    desc: 'Optimal moisture, mild temperature, balanced light'
  },
  {
    name: '🔥 Heatwave',
    soil: 16,
    temp: 36.8,
    rh: 24,
    lux: 48000,
    desc: 'Severe dry soil & intense solar heat -> triggers alert'
  },
  {
    name: '🌊 Overwatered',
    soil: 94,
    temp: 18.2,
    rh: 88,
    lux: 1200,
    desc: 'Waterlogged roots & low light'
  },
  {
    name: '❄️ Cold Shock',
    soil: 38,
    temp: 7.5,
    rh: 45,
    lux: 4500,
    desc: 'Near-freezing temperatures'
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
        // Slowly dry soil by 0.4% every tick, fluctuate temp
        const newSoil = Math.max(8, prev.soil_pct - 0.5);
        const tempDelta = (Math.random() - 0.45) * 0.4;
        const newTemp = Math.min(42, Math.max(10, prev.temperature_c + tempDelta));
        const newRh = Math.min(95, Math.max(15, prev.humidity_pct + (Math.random() - 0.5) * 0.8));
        const newLux = Math.min(65000, Math.max(500, prev.light_lux + (Math.random() - 0.48) * 600));

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
      soil_pct: Math.min(92, prev.soil_pct + 35)
    }));
  };

  return (
    <div className="simulator-card" id="simulator-test-bench">
      <div className="sim-header">
        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
          <Sliders size={18} color="#38bdf8" />
          <span style={{ fontWeight: 700, fontSize: '0.9rem', color: '#fff' }}>
            Virtual Sensor Testbench
          </span>
        </div>
        <span className="sim-badge">MOCK HARDWARE ACTIVE</span>
      </div>

      <p style={{ fontSize: '0.78rem', color: '#94a3b8', marginBottom: 12 }}>
        Test edge AI classification and alert responses without physical sensors connected.
      </p>

      {/* Preset Buttons */}
      <div className="preset-pills">
        {PRESETS.map((p) => (
          <button
            key={p.name}
            className={`preset-btn ${activePreset === p.name ? 'active' : ''}`}
            onClick={() => applyPreset(p)}
          >
            {p.name}
          </button>
        ))}
      </div>

      {/* Sliders for live manual tweaking */}
      <div className="slider-group">
        {/* Soil Moisture Slider */}
        <div className="slider-row">
          <div className="slider-labels">
            <span>Soil Moisture</span>
            <strong>{reading.soil_pct}%</strong>
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
        <div className="slider-row">
          <div className="slider-labels">
            <span>Air Temperature</span>
            <strong>{reading.temperature_c}°C</strong>
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
        <div className="slider-row">
          <div className="slider-labels">
            <span>Relative Humidity</span>
            <strong>{reading.humidity_pct}%</strong>
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
        <div className="slider-row">
          <div className="slider-labels">
            <span>Sunlight</span>
            <strong>{reading.light_lux.toLocaleString()} lx</strong>
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

      {/* Action Controls */}
      <div style={{ display: 'flex', gap: 10, marginTop: 16 }}>
        <button
          className="btn-primary"
          style={{ flex: 1, background: '#0284c7' }}
          onClick={handleWater}
          id="btn-quick-water"
        >
          <Droplet size={15} />
          Pour Water (+35%)
        </button>

        <button
          className="btn-primary"
          style={{
            flex: 1,
            background: isAutoCycling ? '#dc2626' : 'rgba(255, 255, 255, 0.08)',
            border: '1px solid var(--border-subtle)'
          }}
          onClick={() => setIsAutoCycling(!isAutoCycling)}
          id="btn-auto-simulate"
        >
          {isAutoCycling ? <Pause size={15} /> : <Play size={15} />}
          {isAutoCycling ? 'Stop Dynamic Cycle' : 'Auto-Day Cycle'}
        </button>
      </div>
    </div>
  );
}
