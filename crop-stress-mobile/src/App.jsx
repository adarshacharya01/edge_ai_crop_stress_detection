import React, { useState, useMemo } from 'react';
import { 
  Sprout, 
  Activity, 
  Sliders, 
  Settings, 
  AlertTriangle,
  Lightbulb,
  Droplets,
  ArrowRight
} from 'lucide-react';

import StressGauge from './components/StressGauge';
import SensorGrid from './components/SensorGrid';
import SimulatorPanel from './components/SimulatorPanel';
import PlantAndDevice, { CROPS } from './components/PlantAndDevice';

import { calculateStressProbability, diagnoseCropState, CROP_ALERT_THRESHOLD } from './services/cropModel';

export default function App() {
  const [activeTab, setActiveTab] = useState('dashboard');
  const [selectedCrop, setSelectedCrop] = useState(CROPS[0]);

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
  const isWarning = stressScore >= 0.40 && !isAlert;

  // Quick Scenario triggers for main dashboard
  const setQuickScenario = (soil, temp, rh, lux) => {
    setReading({
      soil_pct: soil,
      temperature_c: temp,
      humidity_pct: rh,
      light_lux: lux
    });
  };

  const quickWater = () => {
    setReading((prev) => ({
      ...prev,
      soil_pct: Math.min(88, Math.max(25, prev.soil_pct + 30))
    }));
  };

  return (
    <div className="app-container">
      {/* Top Mobile Header */}
      <header className="app-header">
        <div className="brand-area">
          <div className="brand-icon">
            <Sprout size={18} />
          </div>
          <div>
            <div className="brand-title">SproutGuard</div>
            <div className="brand-subtitle">{selectedCrop.name} · FRDM-MCXW71</div>
          </div>
        </div>

        <button 
          className="connection-pill simulated"
          onClick={() => setActiveTab('simulator')}
          title="Virtual Simulation Active"
        >
          <div className="pulse-dot" />
          <span>Simulated</span>
        </button>
      </header>

      {/* Dynamic Screen Content */}
      <main className="app-content">
        {activeTab === 'dashboard' && (
          <>
            {/* Unified Crop Health Hero Card */}
            <div 
              className="unified-hero-card" 
              style={{ '--hero-accent': diagnosis.color }}
              id="plant-status-banner"
            >
              <div className="hero-left-content">
                <div className="hero-tag-row">
                  <span 
                    className="status-pill-badge"
                    style={{ color: diagnosis.color, background: `${diagnosis.color}15`, borderColor: `${diagnosis.color}35` }}
                  >
                    {diagnosis.badge}
                  </span>
                  <span className="hero-crop-tag" onClick={() => setActiveTab('settings')}>
                    {selectedCrop.name}
                  </span>
                </div>

                <div className="hero-title" style={{ color: diagnosis.color }}>
                  {diagnosis.title}
                </div>

                <div className="hero-advice">
                  {diagnosis.summary}
                </div>
              </div>

              {/* Compact Circular Stress Gauge */}
              <div className="hero-right-gauge">
                <StressGauge score={stressScore} compact={true} />
              </div>
            </div>

            {/* Quick Testing Chips */}
            <div className="quick-test-bar">
              <span className="quick-bar-label">Quick Test:</span>
              <div className="quick-chip-group">
                <button 
                  className="quick-chip"
                  onClick={() => setQuickScenario(52, 23.5, 62, 18000)}
                >
                  🌿 Healthy
                </button>
                <button 
                  className="quick-chip"
                  onClick={() => setQuickScenario(16, 29.0, 35, 25000)}
                >
                  💧 Dry
                </button>
                <button 
                  className="quick-chip"
                  onClick={() => setQuickScenario(14, 38.0, 22, 55000)}
                >
                  🔥 Heat
                </button>
                <button 
                  className="quick-chip"
                  onClick={() => setQuickScenario(95, 18.0, 90, 1500)}
                >
                  🌊 Wet
                </button>
                <button 
                  className="quick-chip water"
                  onClick={quickWater}
                  title="Simulate Watering"
                >
                  <Droplets size={12} />
                  +Water
                </button>
              </div>
            </div>

            {/* 4 Sensor Telemetry Cards (Compact Grid) */}
            <SensorGrid reading={reading} />

            {/* High Priority Agronomic Alert (Visible only when stress is elevated) */}
            {(isAlert || isWarning) ? (
              <div className="alert-action-box" style={{ borderColor: `${diagnosis.color}40`, background: `${diagnosis.color}0c` }}>
                <div className="alert-action-header" style={{ color: diagnosis.color }}>
                  {isAlert ? <AlertTriangle size={16} /> : <Lightbulb size={16} />}
                  <span>Recommended Action</span>
                </div>
                <div className="alert-action-text">
                  {diagnosis.actions[0]}
                </div>
              </div>
            ) : (
              <div className="nominal-footer-note">
                <span className="nominal-dot" />
                <span>All telemetry metrics within target biological tolerance.</span>
              </div>
            )}
          </>
        )}

        {/* Tab 2: Virtual Sensor Simulator */}
        {activeTab === 'simulator' && (
          <SimulatorPanel 
            reading={reading} 
            onUpdateReading={setReading} 
          />
        )}

        {/* Tab 3: Plant & Device Settings */}
        {activeTab === 'settings' && (
          <PlantAndDevice 
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
          <Activity size={19} />
          <span>Monitor</span>
        </button>

        <button 
          className={`nav-item ${activeTab === 'simulator' ? 'active' : ''}`}
          onClick={() => setActiveTab('simulator')}
          id="nav-simulator"
        >
          <Sliders size={19} />
          <span>Simulate</span>
        </button>

        <button 
          className={`nav-item ${activeTab === 'settings' ? 'active' : ''}`}
          onClick={() => setActiveTab('settings')}
          id="nav-settings"
        >
          <Settings size={19} />
          <span>Plant & Device</span>
        </button>
      </nav>
    </div>
  );
}
