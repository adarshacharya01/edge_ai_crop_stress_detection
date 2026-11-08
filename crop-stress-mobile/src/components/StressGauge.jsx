import React from 'react';

export default function StressGauge({ score, compact = true }) {
  const isConnected = score !== null && score !== undefined && !isNaN(score);
  const normalizedScore = isConnected ? Math.min(1.0, Math.max(0.0, score)) : 0;
  const pct = isConnected ? Math.round(normalizedScore * 100) : 0;

  // 4-tier status colors
  // Optimal: <= 0.15 (#10b981)
  // Moderate: 0.16 - 0.50 (#f59e0b)
  // High: 0.51 - 0.80 (#f97316)
  // Critical: >= 0.81 (#ef4444)
  let scoreColor = '#64748b';
  let glowColor = 'transparent';
  let statusBadge = 'DISCONNECTED';

  if (isConnected) {
    if (normalizedScore >= 0.81) {
      scoreColor = '#ef4444';
      glowColor = 'rgba(239, 68, 68, 0.35)';
      statusBadge = 'CRITICAL STRESS';
    } else if (normalizedScore >= 0.51) {
      scoreColor = '#f97316';
      glowColor = 'rgba(249, 115, 22, 0.35)';
      statusBadge = 'HIGH STRESS';
    } else if (normalizedScore >= 0.16) {
      scoreColor = '#f59e0b';
      glowColor = 'rgba(245, 158, 11, 0.3)';
      statusBadge = 'MODERATE STRESS';
    } else {
      scoreColor = '#10b981';
      glowColor = 'rgba(16, 185, 129, 0.3)';
      statusBadge = 'OPTIMAL';
    }
  }

  // Circular ring parameters for compact view
  const size = 72;
  const strokeWidth = 6.5;
  const center = size / 2;
  const radius = center - strokeWidth;
  const circumference = 2 * Math.PI * radius; // ~182.2
  const strokeDashoffset = isConnected ? circumference * (1 - normalizedScore) : circumference;

  if (compact) {
    return (
      <div 
        className="compact-gauge-wrap" 
        title={isConnected ? `Edge AI Stress: ${(normalizedScore * 100).toFixed(0)}%` : 'Board Not Connected'}
      >
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
              filter: isConnected ? `drop-shadow(0 0 6px ${glowColor})` : 'none',
              transform: 'rotate(-90deg)',
              transformOrigin: '50% 50%'
            }}
          />
        </svg>

        <div className="compact-gauge-content">
          <span className="compact-gauge-val" style={{ color: scoreColor }}>
            {isConnected ? normalizedScore.toFixed(2) : '--'}
          </span>
          <span className="compact-gauge-lbl">STRESS</span>
        </div>
      </div>
    );
  }

  // Expanded gauge for detailed view
  return (
    <div className="gauge-expanded-card">
      <div className="gauge-header">
        <span className="gauge-title">
          <span style={{ width: 8, height: 8, borderRadius: '50%', background: scoreColor }} />
          AI Stress Index
        </span>
        <span className="status-pill-badge" style={{ color: scoreColor, background: `${scoreColor}18` }}>
          {statusBadge}
        </span>
      </div>

      <div className="gauge-bar-track">
        <div
          className="gauge-bar-fill"
          style={{
            width: `${pct}%`,
            background: isConnected
              ? `linear-gradient(90deg, #10b981 0%, #f59e0b 50%, ${scoreColor} 100%)`
              : 'transparent',
            boxShadow: isConnected ? `0 0 12px ${glowColor}` : 'none'
          }}
        />
        <div 
          className="gauge-threshold-pin" 
          style={{ left: '51%' }}
          title="High Stress Threshold (0.51)"
        />
        <div 
          className="gauge-threshold-pin" 
          style={{ left: '81%', borderColor: '#ef4444' }}
          title="Critical Stress Threshold (0.81)"
        />
      </div>

      <div className="gauge-bar-labels">
        <span>0.00 (Optimal)</span>
        <span style={{ color: normalizedScore >= 0.51 ? '#f97316' : 'var(--text-dim)', fontWeight: 600 }}>
          {isConnected ? `Current: ${normalizedScore.toFixed(2)}` : 'Disconnected'}
        </span>
        <span>1.00 (Critical)</span>
      </div>
    </div>
  );
}
