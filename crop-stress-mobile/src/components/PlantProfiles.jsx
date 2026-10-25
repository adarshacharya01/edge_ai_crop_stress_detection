import React from 'react';
import { Sprout, Check } from 'lucide-react';

const CROPS = [
  {
    id: 'tomato',
    name: 'Tomato (Solanum lycopersicum)',
    category: 'Fruiting Crop',
    moistureTarget: '50% - 75%',
    tempIdeal: '20°C - 27°C',
    lightNeeds: 'Full Sun (30k+ lx)',
    tip: 'Sensitive to erratic watering which causes blossom end rot.'
  },
  {
    id: 'lettuce',
    name: 'Lettuce / Leafy Greens',
    category: 'Greens',
    moistureTarget: '60% - 80%',
    tempIdeal: '16°C - 22°C',
    lightNeeds: 'Moderate (15k - 25k lx)',
    tip: 'Shallow root system; quickly bolts if ambient temperatures exceed 26°C.'
  },
  {
    id: 'basil',
    name: 'Sweet Basil (Ocimum basilicum)',
    category: 'Herb',
    moistureTarget: '45% - 65%',
    tempIdeal: '22°C - 30°C',
    lightNeeds: 'High (25k+ lx)',
    tip: 'Loves heat; very vulnerable to cold drafts below 12°C.'
  },
  {
    id: 'succulent',
    name: 'Succulent / Cactus',
    category: 'Xerophyte',
    moistureTarget: '15% - 35%',
    tempIdeal: '18°C - 32°C',
    lightNeeds: 'Direct Sun (40k+ lx)',
    tip: 'Thrives in dry, well-draining substrate. Extremely vulnerable to overwatering.'
  }
];

export default function PlantProfiles({ selectedCrop, onSelectCrop }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 14 }}>
      <div className="status-hero" style={{ '--hero-accent': '#10b981' }}>
        <div className="status-hero-top">
          <span className="status-badge" style={{ color: '#10b981' }}>Species Database</span>
          <Sprout size={18} color="#10b981" />
        </div>
        <div className="status-title">Active Crop Profile</div>
        <div className="status-desc">
          Selecting your specific plant species customizes recommended environmental parameters and agronomic recommendations.
        </div>
      </div>

      <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
        {CROPS.map((crop) => {
          const isSelected = selectedCrop.id === crop.id;
          return (
            <div
              key={crop.id}
              onClick={() => onSelectCrop(crop)}
              style={{
                background: isSelected ? 'rgba(16, 185, 129, 0.12)' : 'var(--bg-card)',
                border: isSelected ? '1px solid #10b981' : '1px solid var(--border-subtle)',
                borderRadius: 16,
                padding: 14,
                cursor: 'pointer',
                transition: 'all 0.2s ease'
              }}
            >
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                <div>
                  <div style={{ fontWeight: 700, fontSize: '0.95rem', color: '#fff' }}>
                    {crop.name}
                  </div>
                  <div style={{ fontSize: '0.75rem', color: '#10b981', fontWeight: 600, marginTop: 2 }}>
                    {crop.category}
                  </div>
                </div>
                {isSelected && (
                  <div style={{ background: '#10b981', borderRadius: '50%', width: 22, height: 22, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <Check size={14} color="#000" strokeWidth={3} />
                  </div>
                )}
              </div>

              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8, marginTop: 10, fontSize: '0.75rem' }}>
                <div>
                  <span style={{ color: 'var(--text-dim)' }}>Target Moisture:</span>{' '}
                  <strong style={{ color: '#e2e8f0' }}>{crop.moistureTarget}</strong>
                </div>
                <div>
                  <span style={{ color: 'var(--text-dim)' }}>Ideal Temp:</span>{' '}
                  <strong style={{ color: '#e2e8f0' }}>{crop.tempIdeal}</strong>
                </div>
              </div>

              <div style={{ fontSize: '0.72rem', color: 'var(--text-muted)', marginTop: 8, fontStyle: 'italic' }}>
                Tip: {crop.tip}
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
