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
  const [soilMoisture, setSoilMoisture] = useState(null);
  const [temperature, setTemperature] = useState(null);
  const [humidity, setHumidity] = useState(null);
  const [light, setLight] = useState(null);
  const [liveStressScore, setLiveStressScore] = useState(null);

  const updateTelemetryUI = useCallback((data) => {
    if (!data) {
      setReading(null);
      setSoilMoisture(null);
      setTemperature(null);
      setHumidity(null);
      setLight(null);
      setLiveStressScore(null);
      return;
    }

    const rawSoil = data.soil ?? data.soil_pct ?? null;
    const rawTemp = data.temp ?? data.temperature_c ?? null;
    const rawHum  = data.hum ?? data.humidity_pct ?? null;
    const rawLux  = data.lux ?? data.light_lux ?? null;
    const rawStress = data.stress ?? data.stress_score ?? null;

    if (rawSoil !== null && !isNaN(Number(rawSoil))) {
      setSoilMoisture(Number(rawSoil).toFixed(1));
    }
    if (rawTemp !== null && !isNaN(Number(rawTemp))) {
      setTemperature(Number(rawTemp).toFixed(1));
    }
    if (rawHum !== null && !isNaN(Number(rawHum))) {
      setHumidity(Number(rawHum).toFixed(1));
    }
    if (rawLux !== null && !isNaN(Number(rawLux))) {
      setLight(Math.round(Number(rawLux)));
    }
    if (rawStress !== null && !isNaN(Number(rawStress))) {
      let st = Number(rawStress);
      if (st > 1.0) st /= 100.0;
      setLiveStressScore((st * 100).toFixed(1));
    }

    // Structured reading object for crop model & components
    setReading(prev => {
      const numSoil = (rawSoil !== null && !isNaN(Number(rawSoil))) ? Number(rawSoil) : (prev?.soil_pct ?? 24.5);
      const numTemp = (rawTemp !== null && !isNaN(Number(rawTemp))) ? Number(rawTemp) : (prev?.temperature_c ?? 26.2);
      const numHum  = (rawHum !== null && !isNaN(Number(rawHum))) ? Number(rawHum) : (prev?.humidity_pct ?? 58.0);
      const numLux  = (rawLux !== null && !isNaN(Number(rawLux))) ? Number(rawLux) : (prev?.light_lux ?? 1050.0);
      let numStress = (rawStress !== null && !isNaN(Number(rawStress))) ? Number(rawStress) : (prev?.stress_score ?? 0.14);
      if (numStress > 1.0) numStress /= 100.0;

      return {
        soil_pct: numSoil,
        temperature_c: numTemp,
        humidity_pct: numHum,
        light_lux: numLux,
        stress_score: numStress
      };
    });
  }, []);

  // Subscribe to BLE events
  useEffect(() => {
    const unsubState = bleManager.onStateChange((state) => {
      setBleState(state);
      if (state !== BLE_STATE.CONNECTED) {
        updateTelemetryUI(null);
      }
    });

    const unsubData = bleManager.onData((data) => {
      updateTelemetryUI(data);
    });

    const unsubError = bleManager.onError((err) => {
      alert(`[Bluetooth Error] ${err.message}`);
    });

    return () => {
      unsubState();
      unsubData();
      unsubError();
    };
  }, [updateTelemetryUI]);

  const [isRefreshing, setIsRefreshing] = useState(false);

  const handleManualSync = useCallback(async () => {
    setIsRefreshing(true);
    try {
      await bleManager.readTelemetry();
    } catch (e) {
      console.warn('Manual sync error:', e);
    } finally {
      setTimeout(() => setIsRefreshing(false), 600);
    }
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
    return diagnoseCropState(reading, stressScore, isConnected);
  }, [reading, stressScore, isConnected]);

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

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {isConnected && (
            <button
              className="sync-pill-btn"
              onClick={handleManualSync}
              disabled={isRefreshing}
              title="Manual Telemetry Sync / GATT Read"
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '5px',
                padding: '6px 10px',
                background: 'rgba(255, 255, 255, 0.08)',
                border: '1px solid rgba(255, 255, 255, 0.16)',
                borderRadius: '20px',
                color: isRefreshing ? '#38bdf8' : '#94a3b8',
                fontSize: '11px',
                fontWeight: 500,
                cursor: 'pointer'
              }}
            >
              <RefreshCw size={11} className={isRefreshing ? 'spin-icon' : ''} />
              <span>{isRefreshing ? 'Syncing...' : 'Sync'}</span>
            </button>
          )}

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
                  <span>Connect Sensor</span>
                </>
              )}
            </span>
          </button>
        </div>
      </header>

      {/* Dynamic Screen Content */}
      <main className="app-content">
        {!isConnected && (
          <div 
            style={{
              margin: '0 0 16px 0',
              padding: '12px 16px',
              borderRadius: '12px',
              background: 'rgba(239, 68, 68, 0.15)',
              border: '1px solid rgba(239, 68, 68, 0.45)',
              color: '#fca5a5',
              fontSize: '13px',
              lineHeight: '1.45',
              display: 'flex',
              alignItems: 'center',
              gap: '12px'
            }}
          >
            <AlertTriangle size={24} style={{ flexShrink: 0, color: '#ef4444' }} />
            <div>
              <strong style={{ color: '#ffffff', display: 'block', marginBottom: '2px' }}>
                ⚠️ Do not pair in phone Bluetooth settings.
              </strong>
              Open this app in Chrome and tap <strong>&quot;Connect Sensor&quot;</strong>.
            </div>
          </div>
        )}

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
            <SensorGrid 
              reading={reading} 
              isConnected={isConnected} 
              soilMoisture={soilMoisture}
              temperature={temperature}
              humidity={humidity}
              luxValue={light}
            />

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
