import React from 'react';
import { AlertTriangle, CheckCircle2, AlertCircle } from 'lucide-react';
import { CROP_ALERT_THRESHOLD } from '../services/cropModel';

export default function StressGauge({ score, compact = true }) {
  // Score is 0.00 to 1.00
  const normalizedScore = Math.min(1.0, Math.max(0.0, score || 0));
  const pct = Math.round(normalizedScore * 100);

  // Dynamic status colors
  const isAlert = normalizedScore >= CROP_ALERT_THRESHOLD;
  const isWarning = normalizedScore >= 0.40 && !isAlert;
  const scoreColor = isAlert ? '#ef4444' : isWarning ? '#f59e0b' : '#10b981';
  const glowColor = isAlert ? 'rgba(239, 68, 68, 0.35)' : isWarning ? 'rgba(245, 158, 11, 0.3)' : 'rgba(16, 185, 129, 0.3)';

  // Circular ring parameters for compact view
  const size = 72;
  const strokeWidth = 6.5;
  const center = size / 2;
  const radius = center - strokeWidth;
  const circumference = 2 * Math.PI * radius; // ~182.2
  const strokeDashoffset = circumference * (1 - normalizedScore);

  if (compact) {
    return (
      <div className="compact-gauge-wrap" title={`Edge AI Stress: ${(normalizedScore * 100).toFixed(0)}%`}>
        <svg width={size} height={size} viewBox={`0 0 ${size} ${size}`} className="compact-gauge-svg">
          <circle
            cx={center}
            cy={center}
            r={radius}
            fill="none"
            stroke="rgba(255, 255, 255, 0.08)"
            strokeWidth={strokeWidth}
          />
          <circle
            cx={center}
            cy={center}
            r={radius}
            fill="none"
            stroke={scoreColor}
            strokeWidth={strokeWidth}
            strokeDasharray={circumference}
            strokeDashoffset={strokeDashoffset}
            strokeLinecap="round"
            style={{
              transition: 'stroke-dashoffset 0.6s cubic-bezier(0.4, 0, 0.2, 1), stroke 0.3s ease',
              filter: `drop-shadow(0 0 6px ${glowColor})`,
              transform: 'rotate(-90deg)',
              transformOrigin: '50% 50%'
            }}
          />
        </svg>

        <div className="compact-gauge-content">
          <span className="compact-gauge-val" style={{ color: scoreColor }}>
            {normalizedScore.toFixed(2)}
          </span>
          <span className="compact-gauge-lbl">STRESS</span>
        </div>
      </div>
    );
  }

  // Expanded gauge for detailed view in Testbench
  return (
    <div className="gauge-expanded-card">
      <div className="gauge-header">
        <span className="gauge-title">
          <span style={{ width: 8, height: 8, borderRadius: '50%', background: scoreColor }} />
          AI Stress Index
        </span>
        <span className="status-pill-badge" style={{ color: scoreColor, background: `${scoreColor}18` }}>
          {isAlert ? 'CRITICAL ALERT' : isWarning ? 'ELEVATED' : 'OPTIMAL'}
        </span>
      </div>

      <div className="gauge-bar-track">
        <div
          className="gauge-bar-fill"
          style={{
            width: `${pct}%`,
            background: `linear-gradient(90deg, #10b981 0%, ${normalizedScore > 0.4 ? '#f59e0b' : '#10b981'} 55%, ${scoreColor} 100%)`,
            boxShadow: `0 0 12px ${glowColor}`
          }}
        />
        {/* 0.70 Alert threshold marker */}
        <div 
          className="gauge-threshold-pin" 
          style={{ left: '70%' }}
          title="Alert Threshold (0.70)"
        />
      </div>

      <div className="gauge-bar-labels">
        <span>0.00 (Healthy)</span>
        <span style={{ color: isAlert ? '#ef4444' : 'var(--text-dim)', fontWeight: 600 }}>
          Alert Threshold: 0.70
        </span>
        <span>1.00 (Fatal)</span>
      </div>
    </div>
  );
}
