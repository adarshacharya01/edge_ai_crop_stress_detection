// Unified Plant Profiles Data for SproutGuard AI

export const CROPS = [
  {
    id: 'tomato',
    name: 'Tomato',
    latin: 'Solanum lycopersicum',
    category: 'Fruiting',
    moistureTarget: '50% - 75%',
    tempIdeal: '20°C - 27°C',
    lightNeeds: 'Full Sun (30k+ lx)',
    tip: 'Sensitive to erratic watering (blossom end rot).'
  },
  {
    id: 'lettuce',
    name: 'Leafy Greens',
    latin: 'Lactuca sativa',
    category: 'Greens',
    moistureTarget: '60% - 80%',
    tempIdeal: '16°C - 22°C',
    lightNeeds: 'Moderate (15k - 25k lx)',
    tip: 'Shallow root system; bolts easily above 26°C.'
  },
  {
    id: 'basil',
    name: 'Sweet Basil',
    latin: 'Ocimum basilicum',
    category: 'Herb',
    moistureTarget: '45% - 65%',
    tempIdeal: '22°C - 30°C',
    lightNeeds: 'High (25k+ lx)',
    tip: 'Loves warmth; damaged by drafts below 12°C.'
  },
  {
    id: 'succulent',
    name: 'Succulent',
    latin: 'Crassulaceae',
    category: 'Arid',
    moistureTarget: '15% - 35%',
    tempIdeal: '18°C - 32°C',
    lightNeeds: 'Direct Sun (40k+ lx)',
    tip: 'Drought tolerant; extremely prone to root rot.'
  }
];
