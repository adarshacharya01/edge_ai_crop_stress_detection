import React, { useState } from 'react';
import { Target, CheckCircle, Copy, Cpu, Info } from 'lucide-react';
import { adcMvToSoilPct } from '../services/cropModel';

export default function CalibrationWizard() {
  const [dryMv, setDryMv] = useState(2600);
  const [wetMv, setWetMv] = useState(1100);
  const [testRawMv, setTestRawMv] = useState(1850);
  const [copied, setCopied] = useState(false);

  const calculatedPct = adcMvToSoilPct(testRawMv, dryMv, wetMv);

  const generatedCHeader = 
`// Generated from Mobile Calibration Wizard
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
          <span className="status-badge" style={{ color: '#38bdf8' }}>Hardware Tool</span>
          <Cpu size={18} color="#38bdf8" />
        </div>
        <div className="status-title">Soil Probe Calibration</div>
        <div className="status-desc">
          Capacitive soil sensors output analog voltages. Calibrating dry air vs. submerged water references ensures accurate Soil % readings in both firmware and the app.
        </div>
      </div>

      {/* Step 1: Dry Probe */}
      <div className="wizard-step">
        <div className="wizard-title">
          <span style={{ width: 22, height: 22, borderRadius: '50%', background: '#38bdf8', color: '#000', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '0.75rem' }}>1</span>
          Reference 1: Dry Air Reading (0% Soil Moisture)
        </div>
        <p style={{ fontSize: '0.78rem', color: '#94a3b8' }}>
          Hold sensor completely clean and dry in ambient air. Measure the analog pin voltage with a multimeter or debug logger.
        </p>
        <div className="slider-row">
          <div className="slider-labels">
            <span>Dry Voltage (mV)</span>
            <strong>{dryMv} mV (~{(dryMv / 1000).toFixed(2)} V)</strong>
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
      </div>

      {/* Step 2: Wet Probe */}
      <div className="wizard-step">
        <div className="wizard-title">
          <span style={{ width: 22, height: 22, borderRadius: '50%', background: '#10b981', color: '#000', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '0.75rem' }}>2</span>
          Reference 2: Wet Water Reading (100% Saturated)
        </div>
        <p style={{ fontSize: '0.78rem', color: '#94a3b8' }}>
          Submerge sensor probe up to the white line in a cup of water (do not wet the circuitry). Note the reduced voltage.
        </p>
        <div className="slider-row">
          <div className="slider-labels">
            <span>Wet Voltage (mV)</span>
            <strong>{wetMv} mV (~{(wetMv / 1000).toFixed(2)} V)</strong>
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
      </div>

      {/* Step 3: Interactive Verification */}
      <div className="wizard-step">
        <div className="wizard-title">
          <Target size={18} color="#f59e0b" />
          Live Voltage Test Verification
        </div>
        <p style={{ fontSize: '0.78rem', color: '#94a3b8' }}>
          Drag to test any live ADC voltage and see the converted soil moisture percentage:
        </p>
        <div className="slider-row">
          <div className="slider-labels">
            <span>Simulated Sensor Input: <strong>{testRawMv} mV</strong></span>
            <span style={{ color: '#10b981', fontWeight: 700 }}>Converts to: {calculatedPct}%</span>
          </div>
          <input
            type="range"
            min={wetMv - 100}
            max={dryMv + 100}
            step="10"
            value={testRawMv}
            onChange={(e) => setTestRawMv(parseFloat(e.target.value))}
          />
        </div>
      </div>

      {/* Generated C Code for Firmware */}
      <div className="wizard-step">
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span className="wizard-title">Config for Firmware (Config/config.h)</span>
          <button
            onClick={copyToClipboard}
            style={{
              background: 'none',
              border: 'none',
              color: copied ? '#10b981' : '#38bdf8',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: 4,
              fontSize: '0.75rem',
              fontWeight: 600
            }}
          >
            {copied ? <CheckCircle size={14} /> : <Copy size={14} />}
            {copied ? 'Copied!' : 'Copy Code'}
          </button>
        </div>
        <div className="code-box">
          <pre>{generatedCHeader}</pre>
        </div>
      </div>
    </div>
  );
}
