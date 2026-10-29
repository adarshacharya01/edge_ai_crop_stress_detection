import React, { useState } from 'react';
import { Check, Cpu, Wifi, BatteryCharging, ChevronDown, ChevronUp, Copy, CheckCircle } from 'lucide-react';
import { CROPS } from '../data/cropProfiles';

export default function PlantAndDevice({ selectedCrop, onSelectCrop }) {
  const [showCalibration, setShowCalibration] = useState(false);
  const [dryMv, setDryMv] = useState(2600);
  const [wetMv, setWetMv] = useState(1100);
  const [copied, setCopied] = useState(false);

  const copyHeader = () => {
    const code = `// Soil Calibration Config\n#define CROP_SOIL_DRY_MV ${dryMv}.0f\n#define CROP_SOIL_WET_MV ${wetMv}.0f`;
    navigator.clipboard?.writeText(code);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="plant-device-container">
      {/* 1. Crop Species Selection */}
      <div className="section-block">
        <div className="section-label-row">
          <span className="section-title">Plant Species</span>
          <span className="section-badge">{selectedCrop.name}</span>
        </div>

        <div className="crop-selector-grid">
          {CROPS.map((crop) => {
            const isSelected = selectedCrop.id === crop.id;
            return (
              <div
                key={crop.id}
                className={`crop-card ${isSelected ? 'selected' : ''}`}
                onClick={() => onSelectCrop(crop)}
              >
                <div className="crop-card-top">
                  <div>
                    <div className="crop-card-name">{crop.name}</div>
                    <div className="crop-card-cat">{crop.category}</div>
                  </div>
                  {isSelected && (
                    <div className="crop-check-icon">
                      <Check size={13} strokeWidth={3} />
                    </div>
                  )}
                </div>

                <div className="crop-card-specs">
                  <span>Moisture: <strong>{crop.moistureTarget}</strong></span>
                  <span>Temp: <strong>{crop.tempIdeal}</strong></span>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      {/* 2. Hardware / Edge Device Card */}
      <div className="section-block">
        <span className="section-title">Edge Device Info</span>
        <div className="hardware-card">
          <div className="hardware-row">
            <div className="hw-info-left">
              <Cpu size={16} color="#38bdf8" />
              <span>Target Microcontroller</span>
            </div>
            <strong style={{ color: '#fff', fontSize: '0.8rem' }}>NXP FRDM-MCXW71</strong>
          </div>

          <div className="hardware-row">
            <div className="hw-info-left">
              <Wifi size={16} color="#10b981" />
              <span>Telemetry Link</span>
            </div>
            <span className="hw-badge-live">BLE 5.3 Active</span>
          </div>

          <div className="hardware-row">
            <div className="hw-info-left">
              <BatteryCharging size={16} color="#f59e0b" />
              <span>Edge AI Latency</span>
            </div>
            <strong style={{ color: '#34d399', fontSize: '0.8rem' }}>11.8 ms (Cortex-M33)</strong>
          </div>
        </div>
      </div>

      {/* 3. Advanced Soil Probe Calibration (Collapsible) */}
      <div className="section-block">
        <button
          className="accordion-header"
          onClick={() => setShowCalibration(!showCalibration)}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span style={{ fontSize: '0.85rem', fontWeight: 600, color: 'var(--text-muted)' }}>
              Advanced Sensor ADC Calibration
            </span>
          </div>
          {showCalibration ? <ChevronUp size={16} /> : <ChevronDown size={16} />}
        </button>

        {showCalibration && (
          <div className="calibration-drawer">
            <p style={{ fontSize: '0.75rem', color: 'var(--text-dim)', marginBottom: 12 }}>
              Calibrate capacitive moisture probe reference voltages for dry air and saturated water.
            </p>

            <div className="sim-slider-card" style={{ marginBottom: 10 }}>
              <div className="sim-slider-top">
                <span className="sim-slider-label">Dry Air Reading (mV)</span>
                <span className="sim-slider-val">{dryMv} mV</span>
              </div>
              <input
                type="range"
                min="2000"
                max="3300"
                step="10"
                value={dryMv}
                onChange={(e) => setDryMv(parseFloat(e.target.value))}
              />
            </div>

            <div className="sim-slider-card" style={{ marginBottom: 12 }}>
              <div className="sim-slider-top">
                <span className="sim-slider-label">Submerged Water Reading (mV)</span>
                <span className="sim-slider-val">{wetMv} mV</span>
              </div>
              <input
                type="range"
                min="800"
                max="1800"
                step="10"
                value={wetMv}
                onChange={(e) => setWetMv(parseFloat(e.target.value))}
              />
            </div>

            <button className="copy-config-btn" onClick={copyHeader}>
              {copied ? <CheckCircle size={14} color="#10b981" /> : <Copy size={14} />}
              <span>{copied ? 'Copied to Clipboard!' : 'Copy C Header Values'}</span>
            </button>
          </div>
        )}
      </div>
    </div>
  );
}
