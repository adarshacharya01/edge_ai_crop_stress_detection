import React from 'react';
import { AlertTriangle, CheckCircle2, AlertCircle } from 'lucide-react';
import { CROP_ALERT_THRESHOLD } from '../services/cropModel';

export default function StressGauge({ score }) {
  // Score is 0.00 to 1.00
  const normalizedScore = Math.min(1.0, Math.max(0.0, score));
  
  // Semicircle gauge parameters
  const radius = 80;
  const strokeWidth = 14;
  const center = 100;
  const circumference = Math.PI * radius; // Half-circle perimeter ~ 251.32
  const strokeDashoffset = circumference * (1 - normalizedScore);

  // Dynamic colors based on score
  const isAlert = normalizedScore >= CROP_ALERT_THRESHOLD;
  const isWarning = normalizedScore >= 0.40 && !isAlert;

  const scoreColor = isAlert ? '#ef4444' : isWarning ? '#f59e0b' : '#10b981';

  return (
    <div className="gauge-card" id="stress-gauge-card">
      <div className="gauge-header">
        <span className="gauge-title">
          <span style={{ width: 8, height: 8, borderRadius: '50%', background: scoreColor }} />
          Edge AI Stress Inference
        </span>
        <span style={{ fontSize: '0.75rem', fontWeight: 700, color: scoreColor }}>
          {isAlert ? 'ALERT (CHECK PLANT)' : isWarning ? 'ELEVATED' : 'NOMINAL'}
        </span>
      </div>

      <div className="gauge-wrapper">
        <svg width="200" height="115" viewBox="0 0 200 115" style={{ overflow: 'visible' }}>
          <defs>
            {/* Color Gradient along the arc */}
            <linearGradient id="gaugeGradient" x1="0%" y1="0%" x2="100%" y2="0%">
              <stop offset="0%" stopColor="#10b981" />
              <stop offset="50%" stopColor="#f59e0b" />
              <stop offset="85%" stopColor="#ef4444" />
            </linearGradient>

            <filter id="gaugeGlow" x="-20%" y="-20%" width="140%" height="140%">
              <feDropShadow dx="0" dy="0" stdDeviation="4" floodColor={scoreColor} floodOpacity="0.4" />
            </filter>
          </defs>

          {/* Background Track Arc */}
          <path
            d="M 20 100 A 80 80 0 0 1 180 100"
            fill="none"
            stroke="rgba(255, 255, 255, 0.08)"
            strokeWidth={strokeWidth}
            strokeLinecap="round"
          />

          {/* Active Filled Arc */}
          <path
            d="M 20 100 A 80 80 0 0 1 180 100"
            fill="none"
            stroke="url(#gaugeGradient)"
            strokeWidth={strokeWidth}
            strokeDasharray={circumference}
            strokeDashoffset={strokeDashoffset}
            strokeLinecap="round"
            filter="url(#gaugeGlow)"
            style={{
              transition: 'stroke-dashoffset 0.6s cubic-bezier(0.4, 0, 0.2, 1)',
            }}
          />

          {/* Alert Threshold Indicator at 0.70 (180 - 0.70*180 = 54 deg) */}
          <line
            x1="100"
            y1="100"
            x2="158"
            y2="42"
            stroke="rgba(239, 68, 68, 0.5)"
            strokeWidth="1.5"
            strokeDasharray="3 3"
          />
        </svg>

        <div className="gauge-center-info">
          <div className="gauge-score" style={{ color: scoreColor }}>
            {normalizedScore.toFixed(2)}
          </div>
          <div className="gauge-label">
            Stress Score (0.00 – 1.00)
          </div>
        </div>
      </div>

      <div className="gauge-threshold-notice">
        {isAlert ? (
          <>
            <AlertTriangle size={14} color="#ef4444" />
            <span style={{ color: '#ef4444', fontWeight: 600 }}>Score &ge; 0.70: Triggered Edge Alert</span>
          </>
        ) : isWarning ? (
          <>
            <AlertCircle size={14} color="#f59e0b" />
            <span>Approaching 0.70 threshold</span>
          </>
        ) : (
          <>
            <CheckCircle2 size={14} color="#10b981" />
            <span>Well below 0.70 alert threshold</span>
          </>
        )}
      </div>
    </div>
  );
}
