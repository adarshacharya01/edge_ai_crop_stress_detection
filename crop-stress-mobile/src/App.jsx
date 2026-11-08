import React, { useState, useEffect, useMemo, useCallback } from 'react';
import { 
  Sprout, 
  Activity, 
  Settings, 
  AlertTriangle,
  Lightbulb,
  Bluetooth,
  BluetoothConnected,
  RefreshCw
} from 'lucide-react';

import StressGauge from './components/StressGauge';
import SensorGrid from './components/SensorGrid';
import PlantAndDevice from './components/PlantAndDevice';
import { CROPS } from './data/cropProfiles';

import { 
  calculateStressProbability, 
  diagnoseCropState, 
  CROP_ALERT_THRESHOLD 
} from './services/cropModel';

import { bleManager, BLE_STATE } from './services/bleManager';

export default function App() {
  const [activeTab, setActiveTab] = useState('dashboard');
  const [selectedCrop, setSelectedCrop] = useState(CROPS[0]);

  // Strict Connection State: DISCONNECTED | SCANNING | CONNECTING | CONNECTED
  const [bleState, setBleState] = useState(bleManager.getState());

  // Current reading state (STRICT: starts at null, ZERO mock data)
  const [reading, setReading] = useState(null);

  // Subscribe to BLE events
  useEffect(() => {
    const unsubState = bleManager.onStateChange((state) => {
      setBleState(state);
      if (state !== BLE_STATE.CONNECTED) {
        setReading(null); // Purge all readings when disconnected
      }
    });

    const unsubData = bleManager.onData((data) => {
      // Bound directly to incoming live hardware frame
      setReading(data);
    });

    const unsubError = bleManager.onError((err) => {
      alert(`[Bluetooth Error] ${err.message}`);
    });

    return () => {
      unsubState();
      unsubData();
      unsubError();
    };
  }, []);

  const handleConnect = useCallback(async () => {
    try {
      await bleManager.connect();
    } catch (e) {
      console.warn('BLE connect rejected or failed', e);
    }
  }, []);

  const handleDisconnect = useCallback(async () => {
    try {
      await bleManager.disconnect();
    } catch (e) {
      console.warn('BLE disconnect error', e);
    }
  }, []);

  const isConnected = bleState === BLE_STATE.CONNECTED;
  const isConnecting = bleState === BLE_STATE.CONNECTING || bleState === BLE_STATE.SCANNING;

  // Compute Edge AI inference score & diagnosis in real time (null when disconnected)
  const stressScore = useMemo(() => {
    if (!isConnected || !reading) return null;
    return reading.stress_score ?? calculateStressProbability(reading);
  }, [isConnected, reading]);

  const diagnosis = useMemo(() => {
    return diagnoseCropState(reading, stressScore);
  }, [reading, stressScore]);

  const isAlert = stressScore !== null && stressScore >= CROP_ALERT_THRESHOLD;
  const isWarning = stressScore !== null && stressScore >= 0.16 && !isAlert;

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

        {/* Dynamic Strict Connection Pill */}
        <button 
          className={`connection-pill ${isConnected ? 'connected' : isConnecting ? 'connecting' : 'disconnected'}`}
          onClick={isConnected ? handleDisconnect : handleConnect}
          disabled={isConnecting}
          title={isConnected ? 'Tap to Disconnect' : 'Tap to Connect FRDM-MCXW71'}
          style={{
            cursor: isConnecting ? 'wait' : 'pointer',
            borderColor: isConnected ? 'rgba(16, 185, 129, 0.4)' : isConnecting ? 'rgba(245, 158, 11, 0.4)' : 'rgba(148, 163, 184, 0.3)',
            background: isConnected ? 'rgba(16, 185, 129, 0.12)' : isConnecting ? 'rgba(245, 158, 11, 0.12)' : 'rgba(100, 116, 139, 0.12)'
          }}
        >
          <div 
            className="pulse-dot" 
            style={{
              background: isConnected ? '#10b981' : isConnecting ? '#f59e0b' : '#64748b'
            }}
          />
          <span style={{ color: isConnected ? '#34d399' : isConnecting ? '#fbbf24' : '#94a3b8', display: 'flex', alignItems: 'center', gap: 4 }}>
            {isConnected ? (
              <>
                <BluetoothConnected size={12} />
                <span>Connected</span>
              </>
            ) : isConnecting ? (
              <>
                <RefreshCw size={12} className="spin-icon" />
                <span>{bleState === BLE_STATE.SCANNING ? 'Scanning...' : 'Connecting...'}</span>
              </>
            ) : (
              <>
                <Bluetooth size={12} />
                <span>Connect</span>
              </>
            )}
          </span>
        </button>
      </header>

      {/* Dynamic Screen Content */}
      <main className="app-content">
        {activeTab === 'dashboard' && (
          <>
            {/* Unified Crop Health Hero Card */}
            <div 
              className="unified-hero-card" 
              style={{ 
                '--hero-accent': diagnosis.color,
                cursor: !isConnected ? 'pointer' : 'default'
              }}
              onClick={!isConnected ? handleConnect : undefined}
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
                  <span className="hero-crop-tag" onClick={(e) => { e.stopPropagation(); setActiveTab('settings'); }}>
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

              {/* Circular Stress Gauge: Shows '--' when disconnected */}
              <div className="hero-right-gauge">
                <StressGauge score={stressScore} compact={true} />
              </div>
            </div>

            {/* 4 Sensor Telemetry Cards (Displays '--' when disconnected) */}
            <SensorGrid reading={reading} />

            {/* Agronomic Alert or Operational Status */}
            {isConnected ? (
              (isAlert || isWarning) ? (
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
              )
            ) : (
              <div 
                className="nominal-footer-note"
                style={{ cursor: 'pointer', borderColor: 'rgba(56, 189, 248, 0.3)', background: 'rgba(56, 189, 248, 0.05)' }}
                onClick={handleConnect}
              >
                <Bluetooth size={14} color="#38bdf8" />
                <span style={{ color: '#38bdf8', fontWeight: 500 }}>
                  Tap here or &quot;Connect&quot; above to pair FRDM-MCXW71 via Bluetooth.
                </span>
              </div>
            )}
          </>
        )}

        {/* Tab 2: Plant & Device Settings */}
        {activeTab === 'settings' && (
          <PlantAndDevice 
            selectedCrop={selectedCrop} 
            onSelectCrop={setSelectedCrop}
            bleState={bleState}
            onConnect={handleConnect}
            onDisconnect={handleDisconnect}
          />
        )}
      </main>

      {/* Bottom Navigation (Cleaned: Zero simulator tab) */}
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
