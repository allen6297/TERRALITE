StartupEvents.registry('biome', event => {
  event.register({
    id:       'base:temperate_forest',
    name:     'Temperate Forest',
    priority: 10,
    rarity:   1.0,
    climate: {
      temperature: { min: 0.30, max: 0.70 },
      humidity:    { min: 0.40, max: 0.90 },
      rainfall:    { min: 0.35, max: 0.85 },
      elevation:   { min: 0.20, max: 0.80 },
      drainage:    { min: 0.30, max: 0.70 },
      waterTable:  { min: 0.20, max: 0.70 },
    },
    terrain:    { baseHeight: 48, heightVariation: 14 },
    surface:    { top: 'base:grass', middle: 'base:dirt', middleDepth: 3, base: 'base:stone' },
    atmosphere: { skyColor: [0.58, 0.78, 0.98], fogColor: [0.75, 0.85, 0.95], waterColor: [0.20, 0.45, 0.80] },
    fertility:  { nitrogen: 0.65, phosphorus: 0.45, potassium: 0.50, magnesium: 0.55, calcium: 0.40, sulfur: 0.20 },
    colors:     { grass: [0.3, 0.76, 0.22], foliage: [0.36, 0.62, 0.18] },
  })

  event.register({
    id:       'base:desert',
    name:     'Desert',
    priority: 10,
    rarity:   1.0,
    climate: {
      temperature: { min: 0.65, max: 1.00 },
      humidity:    { min: 0.00, max: 0.30 },
      rainfall:    { min: 0.00, max: 0.25 },
      elevation:   { min: 0.10, max: 0.70 },
      drainage:    { min: 0.65, max: 1.00 },
      waterTable:  { min: 0.60, max: 1.00 },
    },
    terrain:    { baseHeight: 40, heightVariation: 6 },
    surface:    { top: 'base:sand', middle: 'base:sand', middleDepth: 5, base: 'base:stone' },
    atmosphere: { skyColor: [0.85, 0.80, 0.60], fogColor: [0.90, 0.85, 0.65], waterColor: [0.30, 0.55, 0.75] },
    fertility:  { nitrogen: 0.10, phosphorus: 0.20, potassium: 0.30, magnesium: 0.15, calcium: 0.50, sulfur: 0.35 },
    colors:     { grass: [0.74, 0.70, 0.28], foliage: [0.65, 0.62, 0.22] },
  })

  event.register({
    id:       'base:tundra',
    name:     'Tundra',
    priority: 10,
    rarity:   1.0,
    climate: {
      temperature: { min: 0.0,  max: 0.30 },
      humidity:    { min: 0.1,  max: 0.60 },
      rainfall:    { min: 0.1,  max: 0.50 },
      elevation:   { min: 0.3,  max: 0.90 },
      drainage:    { min: 0.15, max: 0.60 },
      waterTable:  { min: 0.1,  max: 0.50 },
    },
    terrain:    { baseHeight: 54, heightVariation: 10 },
    surface:    { top: 'base:dirt', middle: 'base:stone', middleDepth: 1, base: 'base:stone' },
    atmosphere: { skyColor: [0.4314, 0.4667, 0.949], fogColor: [0.8, 0.88, 0.96], waterColor: [0.15, 0.35, 0.65] },
    fertility:  { nitrogen: 0.20, phosphorus: 0.25, potassium: 0.40, magnesium: 0.30, calcium: 0.35, sulfur: 0.15 },
    colors:     { grass: [0.62, 0.72, 0.48], foliage: [0.52, 0.62, 0.40] },
  })
})
