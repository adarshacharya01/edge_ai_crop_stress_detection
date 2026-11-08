import React, { useState } from 'react';
import { Check, Cpu, Wifi, BatteryCharging, ChevronDown, ChevronUp, Copy, CheckCircle, Bluetooth, BluetoothConnected, RefreshCw } from 'lucide-react';
import { CROPS } from '../data/cropProfiles';
import { BLE_STATE } from '../services/bleManager';

export default function PlantAndDevice({ selectedCrop, onSelectCrop, bleState, onConnect, onDisconnect }) {
  const [showCalibration, setShowCalibration] = useState(false);
  const [dryMv, setDryMv] = useState(470);
  const [wetMv, setWetMv] = useState(180);
  const [copied, setCopied] = useState(false);

  const copyHeader = () => {
    const code = `// HW-103 Soil Calibration Config (Stage 2 Calibrated)\n#define CROP_SOIL_DRY_MV ${dryMv}.0f\n#define CROP_SOIL_WET_MV ${wetMv}.0f`;
    navigator.clipboard?.writeText(code);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const isConnected = bleState === BLE_STATE.CONNECTED;
  const isConnecting = bleState === BLE_STATE.CONNECTING || bleState === BLE_STATE.SCANNING;

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
            <strong style={{ color: '#fff', fontSize: '0.8rem' }}>NXP FRDM-MCXW71 (Cortex-M33 @ 96 MHz)</strong>
          </div>

          <div className="hardware-row">
            <div className="hw-info-left">
              {isConnected ? <BluetoothConnected size={16} color="#10b981" /> : <Bluetooth size={16} color="#94a3b8" />}
              <span>Telemetry Link</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
              <span className={`hw-badge-live ${isConnected ? 'connected' : 'disconnected'}`}
                style={{
                  background: isConnected ? 'rgba(16, 185, 129, 0.15)' : 'rgba(100, 116, 139, 0.15)',
                  color: isConnected ? '#34d399' : '#94a3b8',
                  border: `1px solid ${isConnected ? 'rgba(16, 185, 129, 0.3)' : 'rgba(100, 116, 139, 0.3)'}`
                }}
              >
                {bleState}
              </span>
              <button 
                onClick={isConnected ? onDisconnect : onConnect}
                disabled={isConnecting}
                style={{
                  padding: '4px 8px',
                  borderRadius: 6,
                  border: '1px solid rgba(255,255,255,0.1)',
                  background: 'rgba(255,255,255,0.06)',
                  color: '#fff',
                  fontSize: '0.72rem',
                  cursor: 'pointer'
                }}
              >
                {isConnected ? 'Disconnect' : isConnecting ? 'Connecting...' : 'Connect'}
              </button>
            </div>
          </div>

          <div className="hardware-row">
            <div className="hw-info-left">
              <BatteryCharging size={16} color="#f59e0b" />
              <span>Edge AI Latency</span>
            </div>
            <strong style={{ color: isConnected ? '#34d399' : '#94a3b8', fontSize: '0.8rem' }}>
              {isConnected ? '11.8 ms (Live Inference)' : 'Offline'}
            </strong>
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
              HW-103 Soil Probe ADC Calibration
            </span>
          </div>
          {showCalibration ? <ChevronUp size={16} /> : <ChevronDown size={16} />}
        </button>

        {showCalibration && (
          <div className="calibration-panel" style={{ marginTop: 12, padding: 14, background: 'rgba(255,255,255,0.03)', borderRadius: 10, border: '1px solid rgba(255,255,255,0.06)' }}>
            <p style={{ fontSize: '0.78rem', color: 'var(--text-dim)', marginBottom: 12 }}>
              Linear mapping formula for LPADC0 A0 / PTD1:
              <br />
              <code style={{ color: '#38bdf8' }}>Moisture % = clamp( (470.0 - Raw_mV) / (470.0 - 180.0) * 100.0, 0.0, 100.0 )</code>
            </p>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 10, marginBottom: 12 }}>
              <div>
                <label style={{ fontSize: '0.72rem', color: 'var(--text-dim)', display: 'block', marginBottom: 4 }}>
                  Dry Air Baseline (0%):
                </label>
                <input
                  type="number"
                  value={dryMv}
                  onChange={(e) => setDryMv(Number(e.target.value))}
                  style={{ width: '100%', padding: '6px 8px', borderRadius: 6, background: 'rgba(0,0,0,0.3)', border: '1px solid rgba(255,255,255,0.1)', color: '#fff' }}
                />
              </div>
              <div>
                <label style={{ fontSize: '0.72rem', color: 'var(--text-dim)', display: 'block', marginBottom: 4 }}>
                  Saturated Baseline (100%):
                </label>
                <input
                  type="number"
                  value={wetMv}
                  onChange={(e) => setWetMv(Number(e.target.value))}
                  style={{ width: '100%', padding: '6px 8px', borderRadius: 6, background: 'rgba(0,0,0,0.3)', border: '1px solid rgba(255,255,255,0.1)', color: '#fff' }}
                />
              </div>
            </div>

            <button
              onClick={copyHeader}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: 6,
                padding: '6px 12px',
                borderRadius: 6,
                border: '1px solid rgba(56, 189, 248, 0.3)',
                background: 'rgba(56, 189, 248, 0.1)',
                color: '#38bdf8',
                fontSize: '0.76rem',
                cursor: 'pointer'
              }}
            >
              {copied ? <CheckCircle size={14} /> : <Copy size={14} />}
              <span>{copied ? 'Copied config.h snippet!' : 'Copy config.h Calibration'}</span>
            </button>
          </div>
        )}
      </div>
    </div>
  );
}
