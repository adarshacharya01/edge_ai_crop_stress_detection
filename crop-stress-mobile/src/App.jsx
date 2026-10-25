import React, { useState, useMemo } from 'react';
import { 
  Sprout, 
  Activity, 
  Sliders, 
  Target, 
  Radio, 
  CheckCircle2, 
  AlertTriangle,
  Lightbulb,
  Layers,
  ChevronRight,
  Info
} from 'lucide-react';

import StressGauge from './components/StressGauge';
import SensorGrid from './components/SensorGrid';
import SimulatorPanel from './components/SimulatorPanel';
import CalibrationWizard from './components/CalibrationWizard';
import PlantProfiles from './components/PlantProfiles';

import { calculateStressProbability, diagnoseCropState, CROP_ALERT_THRESHOLD } from './services/cropModel';

export default function App() {
  const [activeTab, setActiveTab] = useState('dashboard');
  const [selectedCrop, setSelectedCrop] = useState({
    id: 'tomato',
    name: 'Tomato (Solanum lycopersicum)',
    category: 'Fruiting Crop',
    moistureTarget: '50% - 75%',
    tempIdeal: '20°C - 27°C',
    lightNeeds: 'Full Sun (30k+ lx)',
    tip: 'Sensitive to erratic watering which causes blossom end rot.'
  });

  // Current reading state (Starts in healthy baseline)
  const [reading, setReading] = useState({
    soil_pct: 52.4,
    temperature_c: 24.0,
    humidity_pct: 61.5,
    light_lux: 18500
  });

  // Compute Edge AI inference score & diagnosis in real time
  const stressScore = useMemo(() => {
    return calculateStressProbability(reading);
  }, [reading]);

  const diagnosis = useMemo(() => {
    return diagnoseCropState(reading, stressScore);
  }, [reading, stressScore]);

  const isAlert = stressScore >= CROP_ALERT_THRESHOLD;

  return (
    <div className="app-container">
      {/* Top Mobile Header */}
      <header className="app-header">
        <div className="brand-area">
          <div className="brand-icon">
            <Sprout size={20} />
          </div>
          <div>
            <div className="brand-title">SproutGuard AI</div>
            <div className="brand-subtitle">FRDM-MCXW71 Edge Monitor</div>
          </div>
        </div>

        <div 
          className="connection-pill simulated"
          onClick={() => setActiveTab('simulator')}
          title="Virtual Sensor Mode"
        >
          <div className="pulse-dot" />
          <span>Simulated</span>
        </div>
      </header>

      {/* Dynamic Screen Content */}
      <main className="app-content">
        {activeTab === 'dashboard' && (
          <>
            {/* Status Hero Card */}
            <div 
              className="status-hero" 
              style={{ '--hero-accent': diagnosis.color }}
              id="plant-status-banner"
            >
              <div className="status-hero-top">
                <span 
                  className="status-badge"
                  style={{ color: diagnosis.color, border: `1px solid ${diagnosis.color}40` }}
                >
                  {diagnosis.badge}
                </span>
                <span style={{ fontSize: '0.75rem', color: 'var(--text-muted)' }}>
                  {selectedCrop.name.split(' ')[0]}
                </span>
              </div>
              <div className="status-title" style={{ color: diagnosis.color }}>
                {diagnosis.title}
              </div>
              <div className="status-desc">
                {diagnosis.summary}
              </div>
            </div>

            {/* Edge AI Semicircular Gauge */}
            <StressGauge score={stressScore} />

            {/* 4 Sensor Telemetry Cards */}
            <SensorGrid reading={reading} />

            {/* Plant Doctor Action Plan */}
            <div className="action-box">
              <div className="action-header">
                <Lightbulb size={18} color="#f59e0b" />
                <span>Agronomic Action Plan</span>
              </div>
              <ul className="action-list">
                {diagnosis.actions.map((action, idx) => (
                  <li key={idx} className="action-item">
                    <span 
                      className="action-item-dot" 
                      style={{ background: diagnosis.color }}
                    />
                    <span>{action}</span>
                  </li>
                ))}
              </ul>
            </div>

            {/* Fast Test Button Banner */}
            <div 
              onClick={() => setActiveTab('simulator')}
              style={{
                background: 'rgba(56, 189, 248, 0.08)',
                border: '1px dashed rgba(56, 189, 248, 0.3)',
                borderRadius: 14,
                padding: '12px 16px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                cursor: 'pointer'
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
                <Sliders size={18} color="#38bdf8" />
                <div>
                  <div style={{ fontSize: '0.82rem', fontWeight: 600, color: '#fff' }}>
                    Want to test different stress conditions?
                  </div>
                  <div style={{ fontSize: '0.72rem', color: '#94a3b8' }}>
                    Open Virtual Testbench to simulate heatwaves & dry soil
                  </div>
                </div>
              </div>
              <ChevronRight size={16} color="#38bdf8" />
            </div>
          </>
        )}

        {/* Tab 2: Virtual Sensor Simulator */}
        {activeTab === 'simulator' && (
          <SimulatorPanel 
            reading={reading} 
            onUpdateReading={setReading} 
          />
        )}

        {/* Tab 3: Calibration Wizard */}
        {activeTab === 'calibrate' && (
          <CalibrationWizard />
        )}

        {/* Tab 4: Plant Species Profiles */}
        {activeTab === 'profiles' && (
          <PlantProfiles 
            selectedCrop={selectedCrop} 
            onSelectCrop={setSelectedCrop} 
          />
        )}
      </main>

      {/* Bottom Navigation */}
      <nav className="bottom-nav">
        <button 
          className={`nav-item ${activeTab === 'dashboard' ? 'active' : ''}`}
          onClick={() => setActiveTab('dashboard')}
          id="nav-dashboard"
        >
          <Activity size={20} />
          <span>Dashboard</span>
        </button>

        <button 
          className={`nav-item ${activeTab === 'simulator' ? 'active' : ''}`}
          onClick={() => setActiveTab('simulator')}
          id="nav-simulator"
        >
          <Sliders size={20} />
          <span>Testbench</span>
        </button>

        <button 
          className={`nav-item ${activeTab === 'calibrate' ? 'active' : ''}`}
          onClick={() => setActiveTab('calibrate')}
          id="nav-calibrate"
        >
          <Target size={20} />
          <span>Calibrate</span>
        </button>

        <button 
          className={`nav-item ${activeTab === 'profiles' ? 'active' : ''}`}
          onClick={() => setActiveTab('profiles')}
          id="nav-profiles"
        >
          <Sprout size={20} />
          <span>Species</span>
        </button>
      </nav>
    </div>
  );
}
