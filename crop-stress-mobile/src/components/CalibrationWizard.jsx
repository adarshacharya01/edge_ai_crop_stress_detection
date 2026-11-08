import React, { useState } from 'react';
import { Target, CheckCircle, Copy, Cpu } from 'lucide-react';
import { adcMvToSoilPct } from '../services/cropModel';

export default function CalibrationWizard() {
  const [dryMv, setDryMv] = useState(470);
  const [wetMv, setWetMv] = useState(180);
  const [testRawMv, setTestRawMv] = useState(325);
  const [copied, setCopied] = useState(false);

  const calculatedPct = adcMvToSoilPct(testRawMv, dryMv, wetMv);

  const generatedCHeader = 
`// Generated from Mobile Calibration Wizard (HW-103 Probe)
#define CROP_SOIL_DRY_MV ${parseFloat(dryMv).toFixed(1)}f
#define CROP_SOIL_WET_MV ${parseFloat(wetMv).toFixed(1)}f`;

  const copyToClipboard = () => {
    navigator.clipboard?.writeText(generatedCHeader);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
      {/* Overview Info */}
      <div className="status-hero" style={{ '--hero-accent': '#38bdf8' }}>
        <div className="status-hero-top">
          <span className="status-badge" style={{ color: '#38bdf8' }}>Hardware Calibration</span>
          <Cpu size={18} color="#38bdf8" />
        </div>
        <div className="status-title">HW-103 Soil Probe Calibration</div>
        <div className="status-desc">
          HW-103 analog probe calibration on FRDM-MCXW71 LPADC (PTD1 / A0):
          Dry air produces 470 mV (0% moisture), while saturated soil/water produces 180 mV (100% moisture).
        </div>
      </div>

      {/* Step 1: Dry Probe */}
      <div className="wizard-step">
        <div className="wizard-title">
          <span style={{ width: 22, height: 22, borderRadius: '50%', background: '#38bdf8', color: '#000', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '0.75rem' }}>1</span>
          Reference 1: Dry Air Reading (0% Soil Moisture)
        </div>
        <p style={{ fontSize: '0.78rem', color: '#94a3b8' }}>
          Hold sensor probe completely dry in ambient air. Baseline voltage is approximately 470 mV.
        </p>
        <div className="slider-row">
          <div className="slider-labels">
            <span>Dry Voltage (mV)</span>
            <strong>{dryMv} mV (~{(dryMv / 1000).toFixed(2)} V)</strong>
          </div>
          <input
            type="range"
            min="300"
            max="1200"
            step="10"
            value={dryMv}
            onChange={(e) => setDryMv(parseFloat(e.target.value))}
          />
        </div>
      </div>

      {/* Step 2: Wet Probe */}
      <div className="wizard-step">
        <div className="wizard-title">
          <span style={{ width: 22, height: 22, borderRadius: '50%', background: '#10b981', color: '#000', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '0.75rem' }}>2</span>
          Reference 2: Wet Water Reading (100% Saturated)
        </div>
        <p style={{ fontSize: '0.78rem', color: '#94a3b8' }}>
          Submerge sensor probe in water or saturated soil. Baseline voltage drops to approximately 180 mV.
        </p>
        <div className="slider-row">
          <div className="slider-labels">
            <span>Wet Voltage (mV)</span>
            <strong>{wetMv} mV (~{(wetMv / 1000).toFixed(2)} V)</strong>
          </div>
          <input
            type="range"
            min="100"
            max="400"
            step="10"
            value={wetMv}
            onChange={(e) => setWetMv(parseFloat(e.target.value))}
          />
        </div>
      </div>

      {/* Step 3: Voltage Verification */}
      <div className="wizard-step">
        <div className="wizard-title">
          <Target size={18} color="#f59e0b" />
          Voltage Conversion Test
        </div>
        <p style={{ fontSize: '0.78rem', color: '#94a3b8' }}>
          Linear mapping: clamp((470.0 - Raw_mV) / (470.0 - 180.0) * 100.0, 0, 100)
        </p>
        <div className="slider-row">
          <div className="slider-labels">
            <span>Input Voltage: <strong>{testRawMv} mV</strong></span>
            <span style={{ color: '#10b981', fontWeight: 700 }}>Converts to: {calculatedPct.toFixed(1)}%</span>
          </div>
          <input
            type="range"
            min="150"
            max="500"
            step="5"
            value={testRawMv}
            onChange={(e) => setTestRawMv(parseFloat(e.target.value))}
          />
        </div>
      </div>

      {/* Generated C Code for Firmware */}
      <div className="wizard-step">
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span className="wizard-title">Firmware Macro (Config/config.h)</span>
          <button
            onClick={copyToClipboard}
            style={{
              background: 'none',
              border: 'none',
              color: '#38bdf8',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: 4,
              fontSize: '0.8rem'
            }}
          >
            {copied ? <CheckCircle size={14} color="#10b981" /> : <Copy size={14} />}
            <span>{copied ? 'Copied' : 'Copy'}</span>
          </button>
        </div>
        <pre style={{ background: '#0a0d14', padding: 10, borderRadius: 6, fontSize: '0.75rem', color: '#38bdf8', overflowX: 'auto', border: '1px solid #1e293b' }}>
          {generatedCHeader}
        </pre>
      </div>
    </div>
  );
}
